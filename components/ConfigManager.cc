#include <format>

#include <magic_enum.hpp>

#include <components/ConfigManager.h>
#include <components/MemoryManipulator.h>
#include <components/WebsocketManager.h>
#include <models/WsMessage.h>
#include <utils/logger.h>
#include <utils/system.h>

#include <windows.h>

using namespace components;
using namespace magic_enum;
using namespace models;
using namespace std;
using namespace types;
using namespace utils;

ConfigManager::ConfigManager()
    : _shortcutCommit({'K', {Modifier::Alt, Modifier::Ctrl}}),
      _shortcutManualCompletion({VK_RETURN, {Modifier::Alt}}) {
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
    _threadRetrieveProjectDirectory();
    _threadRetrieveSvnDirectory();

    logger::info(format("Configurator is initialized with version: {}", _siVersionString));
}

ConfigManager::~ConfigManager() {
    _isRunning = false;
}

bool ConfigManager::checkCommit(const uint32_t key, const ModifierSet& modifiers) const {
    return key == _shortcutCommit.first && modifiers == _shortcutCommit.second;
}

bool ConfigManager::checkManualCompletion(const uint32_t key, const ModifierSet& modifiers) const {
    return key == _shortcutManualCompletion.first && modifiers == _shortcutManualCompletion.second;
}

SiVersion::Full ConfigManager::version() const {
    return _siVersion;
}

string ConfigManager::reportVersion() const {
    return _siVersionString;
}

void ConfigManager::wsSettingSync(nlohmann::json&& data) {
    if (const auto serverMessage = SettingSyncServerMessage(move(data));
        serverMessage.result == "success") {
        if (const auto shortcutManualCompletionOpt = serverMessage.shortcutManualCompletion();
            shortcutManualCompletionOpt.has_value()) {
            _shortcutManualCompletion = shortcutManualCompletionOpt.value();
        }
    }
}

void ConfigManager::_threadRetrieveProjectDirectory() {
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

void ConfigManager::_threadRetrieveSvnDirectory() {
    thread([this] {
        while (_isRunning) {
            if (auto tempFile = MemoryManipulator::GetInstance()->getCurrentFilePath().lexically_normal();
                !tempFile.empty()) {
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
