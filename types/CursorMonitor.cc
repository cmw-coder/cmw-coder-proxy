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
}

optional<pair<CursorMonitor::CursorPosition, CursorMonitor::CursorPosition>> CursorMonitor::checkPosition() {
    /// These addresses are for source insight 3.X
    const auto currentCursorLineAddress = _baseAddress + 0x1CBEFC;
    const auto currentCursorCharAddress = _baseAddress + 0x1CBF00;
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
    if (this->_lastPosition != cursorPosition) {
        const auto result = make_pair(this->_lastPosition, cursorPosition);
        this->_lastPosition = cursorPosition;
        return result;
    }
    return nullopt;
}
