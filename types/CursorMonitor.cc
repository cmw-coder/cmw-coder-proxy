#include <format>
#include <thread>
#include <stdexcept>

#include <types/Configurator.h>
#include <types/CursorMonitor.h>
#include <utils/logger.h>

#include <windows.h>

using namespace std;
using namespace types;
using namespace utils;

namespace {
    const unordered_map<SiVersion::Major, unordered_map<SiVersion::Minor, tuple<uint64_t, uint64_t>>> addressMap = {
            {SiVersion::Major::V35, {
                                            {SiVersion::Minor::V0076, {0x1CBEFC, 0x1CBF00}},
                                            {SiVersion::Minor::V0086, {0x1BE0CC, 0x1CD3E0}}
                                    }},
            {SiVersion::Major::V40, {
                                            {SiVersion::Minor::V0086, {0x26D938, 0x26D93C}},
                                            {SiVersion::Minor::V0113, {0x27D040, 0x27E35C}},
                                            {SiVersion::Minor::V0132, {0x28B2FC, 0x28B300}}
                                    }},
    };
}

CursorMonitor::CursorMonitor() :
        _sharedProcessHandle(GetCurrentProcess(), CloseHandle) {
    if (!this->_sharedProcessHandle) {
        throw runtime_error("Failed to get current process handle");
    }
    thread([this]() {
        const auto baseAddress = reinterpret_cast<uint32_t>(GetModuleHandle(nullptr));
        const auto [majorVersion, minorVersion] = Configurator::GetInstance()->version();
        try {
            const auto [lineAddress, charAddress] = addressMap.at(majorVersion).at(minorVersion);
            while (this->_isRunning.load()) {
                CursorPosition cursorPosition{};
                ReadProcessMemory(
                        this->_sharedProcessHandle.get(),
                        reinterpret_cast<LPCVOID>(baseAddress + lineAddress),
                        &cursorPosition.line,
                        sizeof(cursorPosition.line),
                        nullptr
                );
                ReadProcessMemory(
                        this->_sharedProcessHandle.get(),
                        reinterpret_cast<LPCVOID>(baseAddress + charAddress),
                        &cursorPosition.character,
                        sizeof(cursorPosition.character),
                        nullptr
                );
                if (this->_lastPosition.load() != cursorPosition) {
                    const auto lastAction = this->_lastAction.load();
                    if (lastAction != UserAction::Idle) {
                        if (this->_handlers.contains(lastAction)) {
                            this->_handlers.at(lastAction)(this->_lastPosition.load(), cursorPosition);
                        }
                        this->_lastAction.store(UserAction::Idle);
                    }
                    this->_lastPosition.store(cursorPosition);
                }
                this_thread::sleep_for(chrono::milliseconds(1));
            }
        } catch (out_of_range &e) {
            logger::error(format("Unsupported Source Insight Version: ", e.what()));
            exit(1);
        }
    }).detach();
}

CursorMonitor::~CursorMonitor() {
    this->_isRunning.store(false);
}

void CursorMonitor::setAction(UserAction userAction) {
    _lastAction.store(userAction);
}
