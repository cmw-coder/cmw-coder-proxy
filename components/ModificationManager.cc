#include <components/InteractionMonitor.h>
#include <components/ModificationManager.h>
#include <utils/logger.h>

using namespace components;
using namespace std;
using namespace types;
using namespace utils;

ModificationManager::ModificationManager() {
    _monitorCurrentFile();
}

ModificationManager::~ModificationManager() {
    _isRunning = false;
}

string ModificationManager::getModifingFiles() const {
    string result;
    shared_lock lock(_modifingFilesMutex);
    for (const auto& [path, lastActiveTime]: _modifingFiles) {
        if (chrono::high_resolution_clock::now() - lastActiveTime < chrono::minutes(60)) {
            result.append(path);
        }
    }
    return result;
}

void ModificationManager::interactionAcceptCompletion(const any&, bool&) {
    try {
        unique_lock lock(_currentModificationMutex);
        _currentFile().acceptCompletion();
    } catch (const runtime_error& e) {
        logger::warn(e.what());
    }
}

void ModificationManager::interactionDeleteInput(const any&, bool&) {
    try {
        unique_lock lock(_currentModificationMutex);
        _currentFile().remove();
    } catch (const runtime_error& e) {
        logger::warn(e.what());
    }
}

void ModificationManager::interactionEnterInput(const any&, bool& needBlockMessage) {
    interactionNormalInput('\n', needBlockMessage);
}

void ModificationManager::interactionNavigateWithKey(const any& data, bool&) {
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

void ModificationManager::interactionNavigateWithMouse(const any& data, bool&) {
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

void ModificationManager::interactionNormalInput(const any& data, bool&) {
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

void ModificationManager::interactionSave(const any&, bool&) {
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

void ModificationManager::interactionSelectionClear(const any&, bool&) {
    try {
        unique_lock lock(_currentModificationMutex);
        _currentFile().selectionClear();
    } catch (const runtime_error& e) {
        logger::warn(e.what());
    }
}

void ModificationManager::interactionSelectionSet(const any& data, bool&) {
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

void ModificationManager::_monitorCurrentFile() {
    thread([this] {
        while (_isRunning) {
            try {
                const auto currentPath = InteractionMonitor::GetInstance()->getFileName();
                bool hasFile; {
                    shared_lock lock(_modifingFilesMutex);
                    hasFile = _modifingFiles.contains(currentPath);
                }
                if (hasFile) {
                    shared_lock lock(_modifingFilesMutex);
                    _modifingFiles.at(currentPath) = chrono::high_resolution_clock::now();
                } else {
                    unique_lock lock(_modifingFilesMutex);
                    _modifingFiles.emplace(currentPath, chrono::high_resolution_clock::now());
                }
            } catch (const runtime_error& e) {
                logger::warn(e.what());
            }
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }).detach();
}
