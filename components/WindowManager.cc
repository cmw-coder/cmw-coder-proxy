#include <components/ConfigManager.h>
#include <components/MemoryManipulator.h>
#include <components/WindowManager.h>
#include <types/keys.h>
#include <utils/logger.h>
#include <utils/system.h>
#include <utils/window.h>

#include <windows.h>

using namespace components;
using namespace models;
using namespace std;
using namespace types;
using namespace utils;

WindowManager::WindowManager() {
    logger::info("WindowManager is initialized");
}

WindowManager::~WindowManager() {
    _isRunning.store(false);
}

bool WindowManager::checkNeedHideWhenLostFocus(const uint32_t windowHandle) {
    if (const auto windowClass = window::getWindowClassName(windowHandle);
        windowClass == "si_Poplist") {
        _popListWindowHandle.store(windowHandle);
    } else if (_currentWindowHandle.load().has_value()) {
        _currentWindowHandle.store(nullopt);
        return true;
    }
    return false;
}

bool WindowManager::checkNeedShowWhenGainFocus(const uint32_t windowHandle) {
    if (_popListWindowHandle > 0) {
        _popListWindowHandle.store(-1);
    } else if (!_currentWindowHandle.load().has_value()) {
        _currentWindowHandle.store(windowHandle);
        _addEditorWindowHandle(windowHandle);
        return true;
    }
    return false;
}

void WindowManager::closeWindowHandle(const uint32_t windowHandle) {
    bool hasAssociatedFileHandle; {
        shared_lock lock(_fileHandleMapMutex);
        hasAssociatedFileHandle = _fileHandleMap.contains(windowHandle);
    }
    if (hasAssociatedFileHandle) {
        unique_lock lock(_fileHandleMapMutex);
        _fileHandleMap.erase(windowHandle);
    }
}

optional<uint32_t> WindowManager::getAssociatedFileHandle(const uint32_t windowHandle) const {
    try {
        shared_lock lock(_fileHandleMapMutex);
        return _fileHandleMap.at(windowHandle);
    } catch (...) {
        return nullopt;
    }
}

tuple<int64_t, int64_t> WindowManager::getClientPosition() const {
    if (const auto currentWindowHandleOpt = _currentWindowHandle.load();
        currentWindowHandleOpt.has_value()) {
        return window::getClientPosition(currentWindowHandleOpt.value());
    }
    return {};
}

optional<uint32_t> WindowManager::getCurrentWindowHandle() const {
    return _currentWindowHandle;
}

bool WindowManager::hasPopListWindow() const {
    return _popListWindowHandle.load() > 0;
}

void WindowManager::interactionPaste(const any&) {
    if (_currentWindowHandle.load().has_value()) {
        _cancelRetrieveInfo();
    }
}

void WindowManager::sendEnd() const {
    if (_currentWindowHandle.load().has_value()) {
        window::sendKeyInput(VK_END);
    }
}

void WindowManager::sendEscape() const {
    if (hasPopListWindow()) {
        window::sendKeyInput(VK_ESCAPE);
    }
}

void WindowManager::sendF13() const {
    if (_currentWindowHandle.load().has_value()) {
        window::sendKeyInput(VK_F13);
    }
}

void WindowManager::sendLeftThenRight() const {
    if (_currentWindowHandle.load().has_value()) {
        window::sendKeyInput(VK_LEFT);
        window::sendKeyInput(VK_RIGHT);
    }
}

bool WindowManager::sendSave() {
    _cancelRetrieveInfo();
    if (_currentWindowHandle.load().has_value()) {
        window::sendKeyInput('S', {Modifier::Ctrl});
    }
    return false;
}

bool WindowManager::sendFocus() const {
    if (_currentWindowHandle.load().has_value()) {
        return SetFocus(reinterpret_cast<HWND>(_currentWindowHandle.load().value()));
    }
    return false;
}

void WindowManager::_addEditorWindowHandle(const uint32_t windowHandle) {
    unique_lock lock(_fileHandleMapMutex);
    _fileHandleMap.emplace(
        windowHandle,
        MemoryManipulator::GetInstance()->getHandle(MemoryAddress::HandleType::File)
    );
}

void WindowManager::_cancelRetrieveInfo() {
    _needRetrieveInfo.store(false);
}
