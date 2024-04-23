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

void ModificationManager::_monitorCurrentFile() {
    thread([this] {
        while (_isRunning) {
            const auto currentPath = MemoryManipulator::GetInstance()->getFileName();
            if (const auto extension = fs::getExtension(currentPath);
                extension == ".c" || extension == ".h") {
                // TODO: Check if this would cause performance issue
                unique_lock lock(_modifingFilesMutex);
                _recentFiles.emplace(currentPath, chrono::high_resolution_clock::now());
            }
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }).detach();
}
