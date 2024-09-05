#include <components/InteractionMonitor.h>
#include <components/MemoryManipulator.h>
#include <components/ModificationManager.h>
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

vector<filesystem::path> ModificationManager::getRecentFiles(const uint32_t limit) const {
    using FileTime = pair<filesystem::path, chrono::high_resolution_clock::time_point>;
    vector<filesystem::path> recentFiles;
    priority_queue<FileTime, vector<FileTime>, decltype([](const auto& a, const auto& b) {
        return a.second > b.second;
    })> pq; {
        shared_lock lock(_modifyingFilesMutex);
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

void ModificationManager::_monitorCurrentFile() {
    thread([this] {
        while (_isRunning) {
            filesystem::path currentPath; {
                const auto interactionLock = InteractionMonitor::GetInstance()->getInteractionLock();
                currentPath = MemoryManipulator::GetInstance()->getCurrentFilePath();
            }
            if (const auto extension = currentPath.extension();
                extension == ".c" || extension == ".h") {
                unique_lock lock(_modifyingFilesMutex);
                _recentFiles.emplace(currentPath, chrono::high_resolution_clock::now());
            }
            this_thread::sleep_for(100ms);
        }
    }).detach();
}
