#include <components/ModificationManager.h>
#include <utils/fs.h>
#include <utils/logger.h>

#include "InteractionMonitor.h"

using namespace components;
using namespace std;
using namespace types;
using namespace utils;

CaretPosition ModificationManager::getCurrentPosition() {
    try {
        shared_lock lock(_currentModificationMutex);
        return _currentFile().getPosition();
    } catch (out_of_range&) {
        return CaretPosition{};
    }
}

void ModificationManager::interactionAcceptCompletion(const std::any&) {
    try {
        unique_lock lock(_currentModificationMutex);
        _currentFile().acceptCompletion();
    } catch (out_of_range&) {}
}

void ModificationManager::interactionCaretUpdate(const any& data) {
    try {
        const auto [newCursorPosition, oldCursorPosition] = any_cast<tuple<CaretPosition, CaretPosition>>(data);
        unique_lock lock(_currentModificationMutex);
        _currentFile().navigate(newCursorPosition);
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionCaretUpdate data: {}", e.what()));
    }
    catch (out_of_range&) {}
}

void ModificationManager::interactionDeleteInput(const any&) {
    try {
        unique_lock lock(_currentModificationMutex);
        _currentFile().remove();
    } catch (out_of_range&) {}
}

void ModificationManager::interactionEnterInput(const any&) {
    interactionNormalInput('\n');
}

void ModificationManager::interactionNavigate(const any& data) {
    try {
        const auto key = any_cast<Key>(data);
        unique_lock lock(_currentModificationMutex);
        _currentFile().navigate(key);
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionNavigate data: {}", e.what()));
    }
    catch (out_of_range&) {}
}

void ModificationManager::interactionNormalInput(const any& data) {
    try {
        const auto keycode = any_cast<char>(data);
        unique_lock lock(_currentModificationMutex);
        _currentFile().add(keycode);
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionNormalInput data: {}", e.what()));
    }
    catch (out_of_range&) {}
}

void ModificationManager::interactionSave(const std::any&) {
    try {
        thread([this] {
            this_thread::sleep_for(chrono::milliseconds(100));
            unique_lock lock(_currentModificationMutex);
            _currentFile().reload();
        }).detach();
    } catch (out_of_range&) {}
}

void ModificationManager::interactionSelectionClear(const std::any&) {
    try {
        unique_lock lock(_currentModificationMutex);
        _currentFile().selectionClear();
    } catch (out_of_range&) {}
}

void ModificationManager::interactionSelectionSet(const std::any& data) {
    try {
        const auto range = any_cast<Range>(data);
        unique_lock lock(_currentModificationMutex);
        _currentFile().selectionSet(range);
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionSelectionSet data: {}", e.what()));
    }
    catch (out_of_range&) {}
}

Modification& ModificationManager::_currentFile() {
    const auto currentPath = InteractionMonitor::GetInstance()->getFileName();
    if (_modificationMap.contains(currentPath)) {
        return _modificationMap.at(currentPath);
    }
    return _modificationMap.emplace(currentPath, currentPath).first->second;
}
