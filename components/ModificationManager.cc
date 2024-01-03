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

CaretPosition ModificationManager::getCurrentPosition() const {
    try {
        shared_lock lock(_currentModificationMutex);
        return _modificationMap.at(_currentTabName).getPosition();
    } catch (out_of_range&) {
        return CaretPosition{};
    }
}

void ModificationManager::interactionAcceptCompletion(const std::any&) {
    try {
        unique_lock lock(_currentModificationMutex);
        _modificationMap.at(_currentTabName).acceptCompletion();
    } catch (out_of_range&) {}
}

void ModificationManager::interactionCaretUpdate(const any& data) {
    try {
        const auto [newCursorPosition, oldCursorPosition] = any_cast<tuple<CaretPosition, CaretPosition>>(data);
        unique_lock lock(_currentModificationMutex);
        _modificationMap.at(_currentTabName).navigate(newCursorPosition);
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionCaretUpdate data: {}", e.what()));
    }
    catch (out_of_range&) {}
}

void ModificationManager::interactionDeleteInput(const any&) {
    try {
        unique_lock lock(_currentModificationMutex);
        _modificationMap.at(_currentTabName).remove();
    } catch (out_of_range&) {}
}

void ModificationManager::interactionEnterInput(const any&) {
    interactionNormalInput('\n');
}

void ModificationManager::interactionNavigate(const any& data) {
    try {
        const auto key = any_cast<Key>(data);
        unique_lock lock(_currentModificationMutex);
        _modificationMap.at(_currentTabName).navigate(key);
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionNavigate data: {}", e.what()));
    }
    catch (out_of_range&) {}
}

void ModificationManager::interactionNormalInput(const any& data) {
    try {
        const auto keycode = any_cast<char>(data);
        unique_lock lock(_currentModificationMutex);
        _modificationMap.at(_currentTabName).add(keycode);
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionNormalInput data: {}", e.what()));
    }
    catch (out_of_range&) {}
}

void ModificationManager::interactionSave(const std::any&) {
    try {
        thread([this] {
            this_thread::sleep_for(chrono::milliseconds(50));
            unique_lock lock(_currentModificationMutex);
            const auto currentTab = _modificationMap.at(_currentTabName);
            _modificationMap.at(_currentTabName).reload();
        }).detach();
    } catch (out_of_range&) {}
}

void ModificationManager::interactionSelectionClear(const std::any&) {
    try {
        unique_lock lock(_currentModificationMutex);
        _modificationMap.at(_currentTabName).selectionClear();
    } catch (out_of_range&) {}
}

void ModificationManager::interactionSelectionSet(const std::any& data) {
    try {
        const auto range = any_cast<Range>(data);
        unique_lock lock(_currentModificationMutex);
        _modificationMap.at(_currentTabName).selectionSet(range);
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionSelectionSet data: {}", e.what()));
    }
    catch (out_of_range&) {}
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
