#include <components/InteractionMonitor.h>
#include <components/MemoryManipulator.h>
#include <components/ModificationManager.h>
#include <utils/fs.h>
#include <utils/logger.h>

using namespace components;
using namespace std;
using namespace types;
using namespace utils;

ModificationManager::ModificationManager() {
    _monitorCurrentFile();

    logger::info("ModificationManager is initialized");
}

ModificationManager::~ModificationManager() {
    _isRunning = false;
}

vector<string> ModificationManager::getRecentFiles(const uint32_t limit) const {
    using FileTime = pair<string, chrono::high_resolution_clock::time_point>;
    vector<string> recentFiles;
    priority_queue<FileTime, vector<FileTime>, decltype([](const auto& a, const auto& b) {
        return a.second > b.second;
    })> pq; {
        shared_lock lock(_modifingFilesMutex);
        for (const auto& file: _recentFiles) {
            pq.emplace(file);
            if (pq.size() > limit) {
                pq.pop();
            }
        }
    }

    while (!pq.empty()) {
        recentFiles.push_back(pq.top().first);
        pq.pop();
    }

    return recentFiles;
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
    const auto currentPath = MemoryManipulator::GetInstance()->getFileName();
    if (_modificationMap.contains(currentPath)) {
        return _modificationMap.at(currentPath);
    }
    return _modificationMap.emplace(currentPath, currentPath).first->second;
}

void ModificationManager::_monitorCurrentFile() {
    thread([this] {
        while (_isRunning) {
            try {
                const auto currentPath = MemoryManipulator::GetInstance()->getFileName();
                if (const auto extension = fs::getExtension(currentPath);
                    extension == ".c" || extension == ".h") {
                    // TODO: Check if this would cause performance issue
                    unique_lock lock(_modifingFilesMutex);
                    _recentFiles.emplace(currentPath, chrono::high_resolution_clock::now());
                }
            } catch (const runtime_error&) {}
            this_thread::sleep_for(chrono::milliseconds(500));
        }
    }).detach();
}
