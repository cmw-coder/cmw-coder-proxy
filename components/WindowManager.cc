#include <components/ConfigManager.h>
#include <components/MemoryManipulator.h>
#include <components/WindowManager.h>
#include <types/Key.h>
#include <utils/logger.h>
#include <utils/window.h>
#include <utils/system.h>

#include <windows.h>

using namespace components;
using namespace models;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    const auto versionText = format("v{}", VERSION_STRING);
    atomic<uint32_t> mainWindowHandle;
}

WindowManager::WindowManager() : _keyHelper(ConfigManager::GetInstance()->version().first) {
    _threadInitMenuHandle();

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

optional<uint32_t> WindowManager::getAssosiatedFileHandle(const uint32_t windowHandle) const {
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
    _cancelRetrieveInfo();
}

void WindowManager::sendF13() const {
    if (const auto currentWindowHandleOpt = _currentWindowHandle.load();
        currentWindowHandleOpt.has_value()) {
        window::sendKeycode(
            currentWindowHandleOpt.value(),
            _keyHelper.toKeycode(Key::F13)
        );
    }
}

void WindowManager::sendLeftThenRight() const {
    if (const auto currentWindowHandleOpt = _currentWindowHandle.load();
        currentWindowHandleOpt.has_value()) {
        window::sendKeycode(
            currentWindowHandleOpt.value(),
            _keyHelper.toKeycode(Key::Left)
        );
        window::sendKeycode(
            currentWindowHandleOpt.value(),
            _keyHelper.toKeycode(Key::Right)
        );
    }
}

bool WindowManager::sendSave() {
    _cancelRetrieveInfo();
    if (const auto currentWindowHandleOpt = _currentWindowHandle.load();
        currentWindowHandleOpt.has_value()) {
        return window::postKeycode(
            currentWindowHandleOpt.value(),
            _keyHelper.toKeycode(Key::S, Modifier::Ctrl)
        );
    }
    return false;
}

bool WindowManager::sendUndo() {
    _cancelRetrieveInfo();
    if (const auto currentWindowHandleOpt = _currentWindowHandle.load();
        currentWindowHandleOpt.has_value()) {
        return window::postKeycode(
            currentWindowHandleOpt.value(),
            _keyHelper.toKeycode(Key::Z, Modifier::Ctrl)
        );
    }
    return false;
}

void WindowManager::setMenuText(const string& text) const {
    if (_menuHandle < 0) {
        return;
    }
    ModifyMenu(
        reinterpret_cast<HMENU>(_menuHandle.load()),
        _menuItemIndex,
        MF_DISABLED,
        _menuItemIndex,
        (_menuBaseText + text).c_str()
    );
    DrawMenuBar(reinterpret_cast<HWND>(mainWindowHandle.load()));
}

void WindowManager::unsetMenuText() const {
    setMenuText(versionText);
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

void WindowManager::_threadInitMenuHandle() {
    thread([this] {
        mainWindowHandle = window::getMainWindowHandle(GetCurrentProcessId());
        while (!mainWindowHandle) {
            this_thread::sleep_for(chrono::milliseconds(10));
            mainWindowHandle = window::getMainWindowHandle(GetCurrentProcessId());
        }
        const auto menuHandle = GetMenu(reinterpret_cast<HWND>(mainWindowHandle.load()));
        _menuHandle.store(reinterpret_cast<int64_t>(menuHandle));
        _menuItemIndex = GetMenuItemCount(menuHandle);
        AppendMenu(
            menuHandle,
            MF_DISABLED,
            _menuItemIndex,
            (_menuBaseText + versionText).c_str()
        );
    }).detach();
}
