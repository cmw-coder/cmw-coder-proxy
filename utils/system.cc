#include <memory>
#include <stdexcept>

#include <utils/system.h>

#include <windows.h>
#include <winver.h>
#include <TlHelp32.h>

using namespace std;
using namespace utils;

namespace {
    constexpr uint64_t fileTime2Timestamp(const FILETIME& fileTile) {
        return static_cast<uint64_t>(fileTile.dwHighDateTime) << 32 | fileTile.dwLowDateTime & 0xFFFFFFFF;
    }
}

string system::formatSystemMessage(const long errorCode) {
    string errorMessage;
    errorMessage.resize(16384);
    const auto errorMessageLength = FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr,
        errorCode,
        0,
        errorMessage.data(),
        errorMessage.size(),
        nullptr
    );
    return errorMessage.substr(0, errorMessageLength > 0 ? errorMessageLength - 1 : 0);
}

optional<string> system::deleteRegValue(const string& subKey, const string& valueName) {
    if (const auto deleteResult = RegDeleteKeyValue(HKEY_CURRENT_USER, subKey.c_str(), valueName.c_str());
        deleteResult != ERROR_SUCCESS) {
        return formatSystemMessage(deleteResult);
    }
    return nullopt;
}

optional<string> system::getEnvironmentVariable(const string& name) {
    constexpr auto valueLength = 32767;
    string value;
    value.resize(valueLength);
    if (const auto getLength = GetEnvironmentVariable(name.c_str(), value.data(), valueLength); getLength) {
        return value.substr(0, getLength);
    }
    return nullopt;
}

unsigned long system::getMainThreadId() {
    DWORD mainThreadId = 0;
    const shared_ptr<void> sharedSnapshotHandle(CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0), CloseHandle);
    if (sharedSnapshotHandle.get() == INVALID_HANDLE_VALUE) {
        return mainThreadId;
    }
    const auto currentProcessId = GetCurrentProcessId();
    uint64_t minCreateTime = UINT64_MAX;
    auto threadEntry = THREADENTRY32{.dwSize = sizeof(THREADENTRY32)};

    for (bool hasThreadEntry = Thread32First(sharedSnapshotHandle.get(), &threadEntry);
         hasThreadEntry && GetLastError() != ERROR_NO_MORE_FILES;
         hasThreadEntry = Thread32Next(sharedSnapshotHandle.get(), &threadEntry)) {
        if (threadEntry.th32OwnerProcessID == currentProcessId) {
            const auto currentThreadId = threadEntry.th32ThreadID;
            const shared_ptr<void> sharedThreadHandle(
                OpenThread(THREAD_QUERY_INFORMATION, TRUE, currentThreadId),
                CloseHandle
            );
            if (sharedThreadHandle) {
                FILETIME afTimes[4] = {0};
                if (GetThreadTimes(sharedThreadHandle.get(), &afTimes[0], &afTimes[1], &afTimes[2], &afTimes[3])) {
                    if (const uint64_t ullTest = fileTime2Timestamp(afTimes[0]); ullTest && ullTest < minCreateTime) {
                        minCreateTime = ullTest;
                        mainThreadId = currentThreadId;
                    }
                }
            }
        }
    }
    return mainThreadId;
}

string system::getModuleFileName(const uint64_t moduleAddress) {
    string moduleFileName;
    moduleFileName.resize(MAX_PATH);
    const auto copiedSize = GetModuleFileName(
        reinterpret_cast<HMODULE>(moduleAddress),
        moduleFileName.data(),
        MAX_PATH
    );
    return moduleFileName.substr(0, copiedSize);
}

optional<string> system::getRegValue(const string& subKey, const string& valueName) {
    HKEY hKey;
    if (const auto openResult = RegOpenKeyEx(HKEY_CURRENT_USER, subKey.c_str(), 0, KEY_QUERY_VALUE, &hKey);
        openResult != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return nullopt;
    }
    DWORD valueLength = 16384;
    string value;
    value.resize(valueLength);
    const auto getResult = RegGetValue(
        hKey,
        nullptr,
        valueName.c_str(),
        RRF_RT_REG_SZ,
        nullptr,
        value.data(),
        &valueLength
    );
    RegCloseKey(hKey);
    if (getResult == ERROR_SUCCESS) {
        return value.substr(0, valueLength > 0 ? valueLength - 1 : 0);
    }
    return nullopt;
}

string system::getSystemPath(const string& relativePath) {
    auto systemDirectory = string(MAX_PATH, 0);
    const auto length = GetSystemDirectory(systemDirectory.data(), MAX_PATH);
    return systemDirectory.substr(0, length) + R"(\)" + relativePath;
}

string system::getUserName() {
    DWORD userNameLength = MAX_PATH;
    string userName;
    userName.resize(userNameLength);
    GetUserName(userName.data(), &userNameLength);
    return userName.substr(0, userNameLength - 1);
}

tuple<int, int, int, int> system::getVersion() {
    string path; {
        path.resize(MAX_PATH);
        path = path.substr(0, GetModuleFileName(nullptr, path.data(), MAX_PATH));
    }
    DWORD verHandle = 0;
    if (const DWORD verSize = GetFileVersionInfoSize(path.c_str(), &verHandle); verSize != 0) {
        string verData;
        verData.resize(verSize);
        if (GetFileVersionInfo(path.c_str(), verHandle, verSize, verData.data())) {
            LPBYTE lpBuffer = nullptr;
            UINT size = 0;
            if (VerQueryValue(verData.c_str(), "\\", reinterpret_cast<void * *>(&lpBuffer), &size)) {
                if (size) {
                    if (const auto* verInfo = reinterpret_cast<VS_FIXEDFILEINFO *>(lpBuffer);
                        verInfo->dwSignature == 0xfeef04bd) {
                        return {
                            verInfo->dwFileVersionMS >> 16 & 0xffff,
                            verInfo->dwFileVersionMS >> 0 & 0xffff,
                            verInfo->dwFileVersionLS >> 16 & 0xffff,
                            verInfo->dwFileVersionLS >> 0 & 0xffff
                        };
                    }
                }
            }
        }
    }
    return {};
}

void system::setEnvironmentVariable(const string& name, const string& value) {
    SetEnvironmentVariable(name.c_str(), value.empty() ? nullptr : value.c_str());
}

void system::setRegValue(const string& subKey, const string& valueName, const string& value) {
    if (const auto setResult = RegSetKeyValue(
        HKEY_CURRENT_USER,
        subKey.c_str(),
        valueName.c_str(),
        REG_SZ,
        value.c_str(),
        value.size()
    ); setResult != ERROR_SUCCESS) {
        throw runtime_error(formatSystemMessage(setResult));
    }
}
