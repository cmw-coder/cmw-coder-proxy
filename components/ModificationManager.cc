#include <components/InteractionMonitor.h>
#include <components/ModificationManager.h>
#include <utils/logger.h>

using namespace components;
using namespace std;
using namespace types;
using namespace utils;

void ModificationManager::interactionAcceptCompletion(const std::any&) {
    try {
        unique_lock lock(_currentModificationMutex);
        _currentFile().acceptCompletion();
    } catch (const runtime_error& e) {
        logger::warn(e.what());
    }
}

void ModificationManager::interactionCaretUpdate(const any& data) {
    try {
        const auto [newCursorPosition, oldCursorPosition] = any_cast<tuple<CaretPosition, CaretPosition>>(data);
        unique_lock lock(_currentModificationMutex);
        _currentFile().navigate(newCursorPosition);
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionCaretUpdate data: {}", e.what()));
    } catch (const runtime_error& e) {
        logger::warn(e.what());
    }
}

void ModificationManager::interactionDeleteInput(const any&) {
    try {
        unique_lock lock(_currentModificationMutex);
        _currentFile().remove();
    } catch (const runtime_error& e) {
        logger::warn(e.what());
    }
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
    } catch (const runtime_error& e) {
        logger::warn(e.what());
    }
}

void ModificationManager::interactionNormalInput(const any& data) {
    try {
        const auto keycode = any_cast<char>(data);
        unique_lock lock(_currentModificationMutex);
        _currentFile().add(keycode);
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionNormalInput data: {}", e.what()));
    } catch (const runtime_error& e) {
        logger::warn(e.what());
    }
}

void ModificationManager::interactionSave(const std::any&) {
    try {
        thread([this] {
            this_thread::sleep_for(chrono::milliseconds(100));
            unique_lock lock(_currentModificationMutex);
            _currentFile().reload();
        }).detach();
    } catch (const runtime_error& e) {
        logger::warn(e.what());
    }
}

void ModificationManager::interactionSelectionClear(const std::any&) {
    try {
        unique_lock lock(_currentModificationMutex);
        _currentFile().selectionClear();
    } catch (const runtime_error& e) {
        logger::warn(e.what());
    }
}

void ModificationManager::interactionSelectionSet(const std::any& data) {
    try {
        const auto range = any_cast<Range>(data);
        unique_lock lock(_currentModificationMutex);
        _currentFile().selectionSet(range);
    } catch (const bad_any_cast& e) {
        logger::log(format("Invalid interactionSelectionSet data: {}", e.what()));
    } catch (const runtime_error& e) {
        logger::warn(e.what());
    }
}

Modification& ModificationManager::_currentFile() {
    const auto currentPath = InteractionMonitor::GetInstance()->getFileName();
    if (_modificationMap.contains(currentPath)) {
        return _modificationMap.at(currentPath);
    }
    return _modificationMap.emplace(currentPath, currentPath).first->second;
}
