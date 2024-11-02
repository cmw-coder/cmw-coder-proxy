#include <format>

#include <magic_enum.hpp>

#include <components/CompletionManager.h>
#include <components/ConfigManager.h>
#include <components/InteractionMonitor.h>
#include <components/MemoryManipulator.h>
#include <components/WebsocketManager.h>
#include <models/WsMessage.h>
#include <utils/logger.h>
#include <utils/system.h>

using namespace components;
using namespace magic_enum;
using namespace models;
using namespace std;
using namespace types;
using namespace utils;

ConfigManager::ConfigManager() {
    if (const auto [major, minor, build, _] = system::getVersion(); major == 3 && minor == 5) {
        _siVersion = make_pair(
            SiVersion::Major::V35,
            enum_cast<SiVersion::Minor>(build).value_or(SiVersion::Minor::Unknown)
        );
        _siVersionString = "_3.50." + format("{:0>{}}", build, 4);
    } else {
        _siVersion = make_pair(
            SiVersion::Major::V40,
            enum_cast<SiVersion::Minor>(build).value_or(SiVersion::Minor::Unknown)
        );
        _siVersionString = "_4.00." + format("{:0>{}}", build, 4);
    }
    _threadMonitorCurrentProjectPath();
    _threadMonitorCurrentFilePath();

    logger::info(format("Configurator is initialized with version: {}", _siVersionString));
}

ConfigManager::~ConfigManager() {
    _isRunning = false;
}

SiVersion::Full ConfigManager::version() const {
    return _siVersion;
}

string ConfigManager::reportVersion() const {
    return _siVersionString;
}

void ConfigManager::_threadMonitorCurrentFilePath() {
    thread([this] {
        while (_isRunning) {
            filesystem::path tempFile; {
                const auto interactionLock = InteractionMonitor::GetInstance()->getInteractionLock();
                tempFile = MemoryManipulator::GetInstance()->getCurrentFilePath().lexically_normal();
            }
            if (!tempFile.empty()) {
                bool isChanged; {
                    shared_lock lock(_currentFilePathMutex);
                    isChanged = tempFile != _currentFilePath;
                }
                if (isChanged && tempFile.is_absolute()) {
                    WebsocketManager::GetInstance()->send(EditorSwitchFileMessage(tempFile));
                    unique_lock lock(_currentFilePathMutex);
                    _currentFilePath = tempFile;
                }
            }
            this_thread::sleep_for(200ms);
        }
    }).detach();
}

void ConfigManager::_threadMonitorCurrentProjectPath() {
    thread([this] {
        while (_isRunning) {
            // TODO: Check if need InteractionMonitor::GetInstance()->getInteractionLock();
            const auto currentProject = MemoryManipulator::GetInstance()->getProjectDirectory();
            bool isSameProject; {
                shared_lock lock(_currentProjectPathMutex);
                isSameProject = currentProject == _currentProjectPath;
            }
            if (!isSameProject) {
                WebsocketManager::GetInstance()->send(EditorSwitchProjectClientMessage(currentProject));
                unique_lock lock(_currentProjectPathMutex);
                _currentProjectPath = currentProject;
            }
            this_thread::sleep_for(1s);
        }
    }).detach();
}
