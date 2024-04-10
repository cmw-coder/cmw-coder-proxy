#include <format>

#include <magic_enum.hpp>

#include <components/ConfigManager.h>
#include <components/MemoryManipulator.h>
#include <components/WebsocketManager.h>
#include <models/WsMessage.h>
#include <utils/logger.h>
#include <utils/system.h>

using namespace components;
using namespace helpers;
using namespace magic_enum;
using namespace models;
using namespace std;
using namespace types;
using namespace utils;

ConfigManager::ConfigManager()
    : _shortcutCommit({Key::K, {Modifier::Ctrl}}),
      _shortcutManualCompletion({Key::Enter, {Modifier::Alt}}) {
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
    _keyHelper = make_unique<KeyHelper>(_siVersion.first);
    _threadRetrieveProjectDirectory();

    logger::info(format("Configurator is initialized with version: {}", _siVersionString));
}

bool ConfigManager::checkCommit(const Key key, const KeyHelper::ModifierSet& modifiers) const {
    return key == _shortcutCommit.first && modifiers == _shortcutCommit.second;
}

bool ConfigManager::checkManualCompletion(const Key key, const KeyHelper::ModifierSet& modifiers) const {
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
            const auto currentProject = MemoryManipulator::GetInstance()->getProjectDirectory();
            bool isSameProject; {
                shared_lock lock(_currentProjectMutex);
                isSameProject = currentProject == _currentProject;
            }
            if (!isSameProject) {
                WebsocketManager::GetInstance()->send(EditorSwitchProjectClientMessage(currentProject));
                unique_lock lock(_currentProjectMutex);
                _currentProject = currentProject;
            }
            this_thread::sleep_for(chrono::seconds(10));
        }
    }).detach();
}
