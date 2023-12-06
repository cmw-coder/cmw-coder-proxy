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

void ModificationManager::interactionDelete(const any&data) {
    try {
        const auto position = any_cast<CaretPosition>(data);
        unique_lock lock(_currentModificationMutex);
        if (auto&currentTab = _modificationMap.at(_currentTabName);
            currentTab.remove(position)) {
            logger::debug("Delete input at " + to_string(position.line) + ", " + to_string(position.character));
        }
        else {
            logger::debug("Delete input failed at " + to_string(position.line) + ", " + to_string(position.character));
        }
    }
    catch (const bad_any_cast&e) {
        logger::log(format("Invalid interactionDelete data: {}", e.what()));
    }
    catch (out_of_range&) {
    }
}

void ModificationManager::interactionNormal(const any&data) {
    try {
        const auto [position, character] = any_cast<tuple<CaretPosition, char>>(data);
        unique_lock lock(_currentModificationMutex);
        if (auto&currentTab = _modificationMap.at(_currentTabName);
            currentTab.add(position, character)) {
            logger::debug("Delete input at " + to_string(position.line) + ", " + to_string(position.character));
        }
    }
    catch (out_of_range&) {
    }
}

void ModificationManager::reloadTab() {
    if (_modificationMap.contains(_currentTabName)) {
        unique_lock lock(_currentModificationMutex);
        _modificationMap.at(_currentTabName).reload();
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
