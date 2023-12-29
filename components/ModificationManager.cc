#include <components/ModificationManager.h>
#include <utils/fs.h>
#include <utils/logger.h>

using namespace components;
using namespace std;
using namespace types;
using namespace utils;

void ModificationManager::addTab(const string& tabName, const string& path) {
    _currentTabName = tabName;
    if (!_modificationMap.contains(_currentTabName)) {
        _modificationMap.emplace(tabName, path);
    }
}

void ModificationManager::instantCaret(const any& data) {
    try {
        const auto [newCursorPosition, oldCursorPosition] = any_cast<tuple<CaretPosition, CaretPosition>>(data);
        unique_lock lock(_currentModificationMutex);
        _modificationMap.at(_currentTabName).navigate(newCursorPosition);
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid instantCaret data: {}", e.what()));
    }
    catch (out_of_range&) {}
}

void ModificationManager::instantDelete(const any&) {
    try {
        unique_lock lock(_currentModificationMutex);
        if (auto& currentTab = _modificationMap.at(_currentTabName);
            currentTab.isSelect()) {
            currentTab.selectRemove();
        } else {
            currentTab.remove();
        }
    } catch (out_of_range&) {}
}

void ModificationManager::instantEnter(const any&) {
    instantNormal('\n');
}

void ModificationManager::instantNavigate(const any& data) {
    try {
        const auto key = any_cast<Key>(data);
        unique_lock lock(_currentModificationMutex);
        _modificationMap.at(_currentTabName).navigate(key);
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid instantNavigate data: {}", e.what()));
    }
    catch (out_of_range&) {}
}

void ModificationManager::instantNormal(const any& data) {
    try {
        const auto keycode = any_cast<char>(data);
        unique_lock lock(_currentModificationMutex);
        if (auto& currentTab = _modificationMap.at(_currentTabName);
            currentTab.isSelect()) {
            currentTab.replace(string(1, keycode));
        } else {
            currentTab.add(keycode);
        }
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid instantNormal data: {}", e.what()));
    }
    catch (out_of_range&) {}
}

void ModificationManager::instantSelect(const std::any& data) {
    try {
        const auto range = any_cast<Range>(data);
        unique_lock lock(_currentModificationMutex);
        _modificationMap.at(_currentTabName).select(range);
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid instantSelect data: {}", e.what()));
    }
    catch (out_of_range&) {}
}

void ModificationManager::instantClearSelect(const std::any&) {
    try {
        unique_lock lock(_currentModificationMutex);
        _modificationMap.at(_currentTabName).clearSelect();
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid instantClearSelect data: {}", e.what()));
    }
    catch (out_of_range&) {}
}

void ModificationManager::reloadTab() {
    if (_modificationMap.contains(_currentTabName)) {
        thread([this] {
            const auto path = _modificationMap.at(_currentTabName).path;
            const auto currentTime = chrono::file_clock::now();
            while (filesystem::last_write_time(path) < currentTime) {
                this_thread::sleep_for(chrono::milliseconds(100));
            } {
                unique_lock lock(_currentModificationMutex);
                _modificationMap.at(_currentTabName).reload();
            }
        }).detach();
    }
}

void ModificationManager::removeTab(const string& tabName) {
    _modificationMap.erase(tabName);
}

bool ModificationManager::switchTab(const string& tabName) {
    if (_modificationMap.contains(tabName)) {
        _currentTabName = tabName;
        return true;
    }
    return false;
}
