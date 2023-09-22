#include <numeric>
#include <thread>
#include <stdexcept>

#include <types/CursorMonitor.h>

#include <windows.h>

using namespace std;
using namespace types;

CursorMonitor::CursorMonitor() :
        _baseAddress(reinterpret_cast<uint64_t>(GetModuleHandle(nullptr))),
        _sharedProcessHandle(GetCurrentProcess(), CloseHandle) {
    if (!this->_sharedProcessHandle) {
        throw runtime_error("Failed to get current process handle");
    }
    thread([this]() {
        /// These addresses are for source insight 3.X
        const auto currentCursorLineAddress = _baseAddress + 0x1CBEFC;
        const auto currentCursorCharAddress = _baseAddress + 0x1CBF00;
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
                if (lastAction != UserAction::Idle) {
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

void CursorMonitor::queueAction(UserAction userAction) {
    if (_lastAction.load() == UserAction::Idle) {
        _lastAction.store(userAction);
    }
}

void CursorMonitor::addHandler(UserAction userAction, CursorMonitor::CallBackFunction function) {
    _handlers[userAction] = std::move(function);
}
