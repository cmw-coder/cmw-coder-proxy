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
    : _shortcutCommit({Key::K, {Modifier::Alt, Modifier::Ctrl}}),
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
    _threadRetrieveProjectDirectory();
    _threadRetrieveSvnDirectory();

    logger::info(format("Configurator is initialized with version: {}", _siVersionString));
}

ConfigManager::~ConfigManager() {
    _isRunning = false;
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
            const auto currentProject = MemoryManipulator::GetInstance()->getProjectDirectory().string();
            bool isSameProject; {
                shared_lock lock(_currentProjectMutex);
                isSameProject = currentProject == _currentProject;
            }
            if (!isSameProject) {
                WebsocketManager::GetInstance()->send(EditorSwitchProjectClientMessage(currentProject));
                unique_lock lock(_currentProjectMutex);
                _currentProject = currentProject;
            }
            this_thread::sleep_for(chrono::seconds(1));
        }
    }).detach();
}

void ConfigManager::_threadRetrieveSvnDirectory() {
    thread([this] {
        while (_isRunning) {
            if (auto tempFolder = filesystem::path(
                MemoryManipulator::GetInstance()->getFileName()
            ).lexically_normal().parent_path(); !tempFolder.empty()) {
                // bool isMismatch; {
                //     shared_lock lock(_currentSvnMutex);
                //     if (_currentSvn.empty()) {
                //         isMismatch = true;
                //     } else {
                //         isMismatch = mismatch(_currentSvn.begin(), _currentSvn.end(), tempFolder.begin())
                //                      .first != _currentSvn.end();
                //     }
                // }
                if (/*isMismatch && */ tempFolder.is_absolute()) {
                    while (!tempFolder.empty()) {
                        if (exists(tempFolder / ".svn")) {
                            WebsocketManager::GetInstance()->send(EditorSwitchSvnClientMessage(tempFolder.string()));
                            // logger::debug(format("Switched to SVN directory: {}", tempFolder.string()));
                            unique_lock lock(_currentSvnMutex);
                            _currentSvn = tempFolder;
                            break;
                        }
                        const auto parentPath = tempFolder.parent_path();
                        if (parentPath == tempFolder) {
                            break;
                        }
                        tempFolder = parentPath;
                    }
                }
            }
            this_thread::sleep_for(chrono::milliseconds(200));
        }
    }).detach();
}
