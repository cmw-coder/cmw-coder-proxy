#include <stdexcept>
#include <thread>

#include <types/CursorMonitor.h>

#include <windows.h>

using namespace std;
using namespace types;

CursorMonitor::CursorMonitor() : _sharedProcessHandle(GetCurrentProcess(), CloseHandle) {
    if (!this->_sharedProcessHandle) {
        throw runtime_error("Failed to get current process handle");
    }
    thread([this]() {
        /// These addresses are for source insight 3.X
        const auto baseAddress = reinterpret_cast<uint64_t>(GetModuleHandle(nullptr));
        const auto currentCursorLineAddress = baseAddress + 0x1CBEFC;
        const auto currentCursorCharAddress = baseAddress + 0x1CBF00;
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
                this->_lastPosition.store(cursorPosition);
                this->_notify();
            }
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }).detach();
}

CursorMonitor::~CursorMonitor() {
    this->_isRunning.store(false);
}

void CursorMonitor::addHandler(CursorMonitor::CallBackFunction function) {
    _handlers.emplace_back(std::move(function));
}

void CursorMonitor::_notify() {
    const auto cursorPosition = this->_lastPosition.load();
    for (const auto &handler: this->_handlers) {
        handler(cursorPosition);
    }
}

