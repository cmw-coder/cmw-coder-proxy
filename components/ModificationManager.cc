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

void ModificationManager::delayedDelete(CaretPosition, const CaretPosition oldPosition, const any&) {
    try {
        unique_lock lock(_currentModificationMutex);
        if (auto& currentTab = _modificationMap.at(_currentTabName);
            currentTab.remove(oldPosition)) {
            logger::debug("Delete input at " + to_string(oldPosition.line) + ", " + to_string(oldPosition.character));
        }
    } catch (out_of_range&) {
    }
}

void ModificationManager::delayedEnter(CaretPosition, const CaretPosition oldPosition, const std::any&) {
    delayedNormal({}, oldPosition, '\n');
}

void ModificationManager::delayedNormal(CaretPosition, const CaretPosition oldPosition, const any& data) {
    try {
        const auto keycode = any_cast<char>(data);
        unique_lock lock(_currentModificationMutex);
        if (auto& currentTab = _modificationMap.at(_currentTabName);
            currentTab.add(oldPosition, keycode)) {
            logger::debug("Normal input at " + to_string(oldPosition.line) + ", " + to_string(oldPosition.character));
        }
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionNormal data: {}", e.what()));
    }
    catch (out_of_range&) {
    }
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

void ModificationManager::removeTab(const std::string& tabName) {
    _modificationMap.erase(tabName);
}

bool ModificationManager::switchTab(const std::string& tabName) {
    if (_modificationMap.contains(tabName)) {
        _currentTabName = tabName;
        return true;
    }
    return false;
}
