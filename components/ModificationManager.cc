#include <components/ModificationManager.h>
#include <utils/fs.h>
#include <utils/logger.h>

using namespace components;
using namespace std;
using namespace types;
using namespace utils;

void ModificationManager::addTab(const string&tabName, const string&path) {
    _currentTabName = tabName;
    if (!_modificationMap.contains(_currentTabName)) {
        _modificationMap.emplace(tabName, path);
    }
}

void ModificationManager::interactionDelete(const CaretPosition position) {
    try {
        unique_lock lock(_currentModificationMutex);
        if (auto&currentTab = _modificationMap.at(_currentTabName);
            currentTab.remove(position)) {
            logger::debug("Delete input at " + to_string(position.line) + ", " + to_string(position.character));
        }
    }
    catch (out_of_range&) {
    }
}

void ModificationManager::interactionNormal(const CaretPosition position, const char character) {
    try {
        unique_lock lock(_currentModificationMutex);
        if (auto&currentTab = _modificationMap.at(_currentTabName);
            currentTab.add(position, character)) {
            logger::debug("Normal input at " + to_string(position.line) + ", " + to_string(position.character));
        }
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

void ModificationManager::removeTab(const std::string&tabName) {
    _modificationMap.erase(tabName);
}

bool ModificationManager::switchTab(const std::string&tabName) {
    if (_modificationMap.contains(tabName)) {
        _currentTabName = tabName;
        return true;
    }
    return false;
}
