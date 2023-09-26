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

CursorMonitor::CursorMonitor() :
        _baseAddress(reinterpret_cast<uint64_t>(GetModuleHandle(nullptr))),
        _sharedProcessHandle(GetCurrentProcess(), CloseHandle) {
    if (!this->_sharedProcessHandle) {
        throw runtime_error("Failed to get current process handle");
    }
    thread([this]() {
        uint64_t currentCursorLineAddress, currentCursorCharAddress;
        if (Configurator::GetInstance()->version() == SiVersion::V350076) {
            currentCursorLineAddress = _baseAddress + 0x1CBEFC;
            currentCursorCharAddress = _baseAddress + 0x1CBF00;
        } else if (Configurator::GetInstance()->version() == SiVersion::V350086) {
            currentCursorLineAddress = _baseAddress + 0x1BE0CC;
            currentCursorCharAddress = _baseAddress + 0x1CD3E0;
        } else {
            throw runtime_error("Unsupported Source Insight version");
        }
        while (this->_isRunning.load()) {
            CursorPosition cursorPosition{};
            ReadProcessMemory(
                    this->_sharedProcessHandle.get(),
                    reinterpret_cast<LPCVOID>(currentCursorLineAddress),
                    &cursorPosition.line,
                    sizeof(cursorPosition.line),
                    nullptr
            );
            ReadProcessMemory(
                    this->_sharedProcessHandle.get(),
                    reinterpret_cast<LPCVOID>(currentCursorCharAddress),
                    &cursorPosition.character,
                    sizeof(cursorPosition.character),
                    nullptr
            );
            if (this->_lastPosition.load() != cursorPosition) {
                const auto lastAction = this->_lastAction.load();
                logger::log(format(
                        "Cursor Moved, lastLine: {}, currentLine: {}",
                        this->_lastPosition.load().line,
                        cursorPosition.line
                ));
                if (lastAction != UserAction::Idle) {
                    if (lastAction == UserAction::DeleteBackward) {
                        logger::log("Cursor Moved due to backspace");
                    }
                    if (this->_handlers.contains(lastAction)) {
                        this->_handlers.at(lastAction)(this->_lastPosition.load(), cursorPosition);
                    }
                    this->_lastAction.store(UserAction::Idle);
                }
                this->_lastPosition.store(cursorPosition);
            }
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }).detach();
}

CursorMonitor::~CursorMonitor() {
    this->_isRunning.store(false);
}

void CursorMonitor::setAction(UserAction userAction) {
    _lastAction.store(userAction);
}
