#include <format>
#include <numeric>
#include <thread>
#include <stdexcept>

#include <types/CursorMonitor.h>
#include <utils/logger.h>

#include <windows.h>

using namespace std;
using namespace types;
using namespace utils;

CursorMonitor::CursorMonitor() :
        _baseAddress(reinterpret_cast<uint64_t>(GetModuleHandle(nullptr))),
        _sharedProcessHandle(GetCurrentProcess(), CloseHandle) {
    if (!_sharedProcessHandle) {
        throw runtime_error("Failed to get current process handle");
    }
    thread([this]{
        /// These addresses are for source insight 3.X
        const auto currentCursorLineAddress = _baseAddress + 0x1CBEFC;
        const auto currentCursorCharAddress = _baseAddress + 0x1CBF00;
        while (_isRunning.load()) {
            CursorPosition cursorPosition{};
            ReadProcessMemory(
                    _sharedProcessHandle.get(),
                    reinterpret_cast<LPCVOID>(currentCursorLineAddress),
                    &cursorPosition.line,
                    sizeof(cursorPosition.line),
                    nullptr
            );
            ReadProcessMemory(
                    _sharedProcessHandle.get(),
                    reinterpret_cast<LPCVOID>(currentCursorCharAddress),
                    &cursorPosition.character,
                    sizeof(cursorPosition.character),
                    nullptr
            );
            if (_lastPosition.load() != cursorPosition) {
                {
                    const unique_lock lock(_positionQueueMutex);
                    _positionQueue.emplace_back(_lastPosition.load(), cursorPosition);
                }
                _lastPosition.store(cursorPosition);
            }
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }).detach();
    thread([this]{
        {
            const unique_lock actionQueueLock(_actionQueueMutex), positionQueueLock(_positionQueueMutex);
            if (!_actionQueue.empty() && !_positionQueue.empty()) {
                const auto userAction = _actionQueue.front();
                const auto [oldPosition, newPosition] = _positionQueue.front();
                _actionQueue.pop_front();
                _positionQueue.pop_front();

                logger::log(format("Popped action. Current: '{}'", accumulate(
                        _actionQueue.begin(), _actionQueue.end(), string(),
                        [](string acc, UserAction cur) {
                            return format("{}, {}", std::move(acc), static_cast<int>(cur));
                        }
                )));

                if (_handlers.contains(userAction)) {
                    _handlers.at(userAction)(oldPosition, newPosition);
                }
            }
        }
    }).detach();
}

CursorMonitor::~CursorMonitor() {
    _isRunning.store(false);
}

void CursorMonitor::queueAction(UserAction userAction) {
    const unique_lock lock(_actionQueueMutex);
    _actionQueue.push_back(userAction);
    logger::log(format("Queued action. Current: '{}'", accumulate(
            _actionQueue.begin(), _actionQueue.end(), string(),
            [](string acc, UserAction cur) {
                return format("{}, {}", std::move(acc), static_cast<int>(cur));
            }
    )));
}

void CursorMonitor::addHandler(UserAction userAction, CursorMonitor::CallBackFunction function) {
    _handlers[userAction] = std::move(function);
}
