#include <memory>
#include <vector>
#include <stdexcept>

#include <utils/system.h>

#include <windows.h>
#include <TlHelp32.h>

using namespace std;
using namespace utils;

namespace {
    constexpr uint64_t fileTime2Timestamp(const FILETIME &fileTile) {
        return static_cast<uint64_t>(fileTile.dwHighDateTime) << 32 | fileTile.dwLowDateTime & 0xFFFFFFFF;
    }
}

string system::getSystemDirectory() {
    string systemDirectory;
    systemDirectory.resize(MAX_PATH);
    GetSystemDirectory(systemDirectory.data(), MAX_PATH);
    systemDirectory.resize(strlen(systemDirectory.data()));
    return systemDirectory;
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
            const shared_ptr<void> sharedThreadHandle(OpenThread(THREAD_QUERY_INFORMATION, TRUE, currentThreadId),
                                                      CloseHandle);
            if (sharedThreadHandle) {
                FILETIME afTimes[4] = {0};
                if (GetThreadTimes(sharedThreadHandle.get(), &afTimes[0], &afTimes[1], &afTimes[2], &afTimes[3])) {
                    uint64_t ullTest = fileTime2Timestamp(afTimes[0]);
                    if (ullTest && ullTest < minCreateTime) {
                        minCreateTime = ullTest;
                        mainThreadId = currentThreadId;
                    }
                }
            }
        }
    }
    return mainThreadId;
}

string system::getModuleFileName(uint64_t moduleAddress) {
    string moduleFileName;
    moduleFileName.resize(MAX_PATH);
    const auto copiedSize = GetModuleFileName(
            reinterpret_cast<HMODULE>(moduleAddress),
            moduleFileName.data(),
            MAX_PATH
    );
    return moduleFileName.substr(0, copiedSize);
}

uint64_t system::scanPattern(const string &pattern) {
    uint64_t varAddress = 0;
    const shared_ptr<void> sharedProcessHandle(GetCurrentProcess(), CloseHandle);
    if (sharedProcessHandle) {
        const auto BaseAddress = reinterpret_cast<uint64_t>(GetModuleHandle(nullptr));
        string scannedString;
        scannedString.resize(1024);
        SIZE_T readLength = 0;
        // Scan Process Memory using ReadProcessMemory
        for (auto i = 0; i < 100000; i++) {
            if (ReadProcessMemory(
                    sharedProcessHandle.get(),
                    (LPCVOID) (BaseAddress + i * scannedString.size() * sizeof(char)),
                    scannedString.data(),
                    scannedString.size() * sizeof(char),
                    &readLength
            )) {
                const auto found = scannedString.find(pattern);
                if (found != string::npos) {
                    varAddress = BaseAddress + i * scannedString.size() * sizeof(char) + found;
                    break;
                }
            }
        }
    }
    return varAddress;
}

bool system::writeMemory(uint64_t address, const string &value) {
    const shared_ptr<void> sharedProcessHandle(GetCurrentProcess(), CloseHandle);
    if (sharedProcessHandle) {
        return WriteProcessMemory(
                sharedProcessHandle.get(),
                reinterpret_cast<LPVOID>(address),
                value.data(),
                (value.length() + 1) * sizeof(char),
                nullptr
        );
    }
    return false;
}

void system::setRegValue32(const string &subKey, const string &valueName, uint32_t value) {
    const auto setResult = RegSetKeyValue(
            HKEY_CURRENT_USER,
            subKey.c_str(),
            valueName.c_str(),
            REG_DWORD,
            &value,
            sizeof(DWORD)
    );
    if (setResult != ERROR_SUCCESS) {
        string errorMessage;
        errorMessage.resize(512);
        const auto errorMessageLength = FormatMessage(
                FORMAT_MESSAGE_FROM_SYSTEM,
                nullptr,
                setResult,
                0,
                errorMessage.data(),
                errorMessage.size(),
                nullptr
        );
        throw (runtime_error(errorMessage.substr(0, errorMessageLength)));
    }
}
