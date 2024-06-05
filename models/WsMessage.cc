#include <magic_enum.hpp>

#include <models/WsMessage.h>
#include <utils/iconv.h>

#include <windows.h>

using namespace helpers;
using namespace magic_enum;
using namespace models;
using namespace std;
using namespace types;
using namespace utils;

WsMessage::WsMessage(const WsAction action): action(action) {}

WsMessage::WsMessage(const WsAction action, Json::Value&& data)
    : action(action), _data(move(data)) {}

string WsMessage::parse() const {
    Json::Value jsonMessage;
    jsonMessage["action"] = string(enum_name(action));

    if (!_data.empty()) {
        jsonMessage["data"] = _data;
    }

    return writeString(Json::StreamWriterBuilder(), jsonMessage);
}

ChatInsertServerMessage::ChatInsertServerMessage(Json::Value&& data)
    : WsMessage(WsAction::CompletionGenerate, move(data)), result(_data["result"].asString()) {
    if (result == "success") {
        _content.emplace(_data["content"].asString());
    } else if (_data.isMember("message")) {
        _message = _data["message"].asString();
    }
}

optional<string> ChatInsertServerMessage::content() const {
    return _content;
}

CompletionAcceptClientMessage::CompletionAcceptClientMessage(const string& actionId, const uint32_t index)
    : WsMessage(WsAction::CompletionAccept) {
    _data["actionId"] = actionId;
    _data["index"] = index;
}

CompletionCacheClientMessage::CompletionCacheClientMessage(const bool isDelete)
    : WsMessage(WsAction::CompletionCache, isDelete) {}

CompletionCancelClientMessage::CompletionCancelClientMessage(const string& actionId, const bool isExplicit)
    : WsMessage(WsAction::CompletionCancel) {
    _data["actionId"] = actionId;
    _data["explicit"] = isExplicit;
}

CompletionEditClientMessage::CompletionEditClientMessage(
    const string& actionId,
    const uint32_t count,
    const string& editedContent,
    const KeptRatio keptRatio
): WsMessage(WsAction::CompletionEdit) {
    _data["actionId"] = actionId;
    _data["count"] = count;
    _data["editedContent"] = editedContent;
    _data["keptRatio"] = string(enum_name(keptRatio));
}

CompletionGenerateClientMessage::CompletionGenerateClientMessage(
    const CaretPosition& caret,
    const filesystem::path& path,
    const string& prefix,
    const vector<filesystem::path>& recentFiles,
    const string& suffix,
    const vector<SymbolInfo>& symbols
): WsMessage(WsAction::CompletionGenerate) {
    _data["caret"]["character"] = caret.character;
    _data["caret"]["line"] = caret.line;
    _data["path"] = iconv::autoDecode(path.generic_string());
    _data["prefix"] = prefix;
    _data["recentFiles"] = Json::arrayValue;
    _data["suffix"] = suffix;
    _data["symbols"] = Json::arrayValue;
    for (const auto& recentFile: recentFiles) {
        _data["recentFiles"].append(iconv::autoDecode(recentFile.generic_string()));
    }
    for (const auto& [path, name, type, startLine, endLine]: symbols) {
        Json::Value symbol;
        symbol["endLine"] = endLine;
        symbol["name"] = name;
        symbol["path"] = iconv::autoDecode(path.generic_string());
        symbol["startLine"] = startLine;
        symbol["type"] = type;
        _data["symbols"].append(move(symbol));
    }
}

CompletionGenerateServerMessage::CompletionGenerateServerMessage(Json::Value&& data)
    : WsMessage(WsAction::CompletionGenerate, move(data)), result(_data["result"].asString()) {
    if (result == "success") {
        if (const auto completionTypeopt = enum_cast<CompletionType>(
            _data["completions"]["type"].asString()
        ); completionTypeopt.has_value()) {
            _type = completionTypeopt.value();
        }
        vector<string> candidates;
        candidates.reserve(_data["completions"]["candidates"].size());
        ranges::transform(
            _data["completions"]["candidates"],
            back_inserter(candidates),
            [](const auto& candidate) { return candidate.asString(); }
        );
        _completionsOpt.emplace(_data["actionId"].asString(), move(candidates));
    } else if (_data.isMember("message")) {
        _message = _data["message"].asString();
    }
}

string CompletionGenerateServerMessage::message() const {
    return _message;
}

optional<Completions> CompletionGenerateServerMessage::completions() const {
    return _completionsOpt;
}

CompletionSelectClientMessage::CompletionSelectClientMessage(
    const string& actionId,
    uint32_t index,
    const int64_t height,
    const int64_t x,
    const int64_t y
): WsMessage(WsAction::CompletionSelect) {
    _data["actionId"] = actionId;
    _data["index"] = index;
    _data["dimensions"]["height"] = height;
    _data["dimensions"]["x"] = x;
    _data["dimensions"]["y"] = y;
}

EditorCommitClientMessage::EditorCommitClientMessage(const filesystem::path& path)
    : WsMessage(WsAction::EditorCommit, iconv::autoDecode(path.generic_string())) {}

EditorFocusStateClientMessage::EditorFocusStateClientMessage(const bool isFocused)
    : WsMessage(WsAction::EditorFocusState, isFocused) {}

EditorPasteClientMessage::EditorPasteClientMessage(const uint32_t count)
    : WsMessage(
        WsAction::EditorPaste, {
            {"count", count},
        }
    ) {}

EditorSwitchProjectClientMessage::EditorSwitchProjectClientMessage(const filesystem::path& path)
    : WsMessage(WsAction::EditorSwitchProject, iconv::autoDecode(path.generic_string())) {}

EditorSwitchSvnClientMessage::EditorSwitchSvnClientMessage(const filesystem::path& path)
    : WsMessage(WsAction::EditorSwitchSvn, iconv::autoDecode(path.generic_string())) {}

HandShakeClientMessage::HandShakeClientMessage(const filesystem::path& currentProject, string&& version)
    : WsMessage(WsAction::HandShake) {
    _data["pid"] = static_cast<uint64_t>(GetCurrentProcessId());
    _data["currentProject"] = iconv::autoDecode(currentProject.generic_string());
    _data["version"] = version;
}

SettingSyncServerMessage::SettingSyncServerMessage(Json::Value&& data)
    : WsMessage(WsAction::SettingSync, move(data)),
      result(_data["result"].asString()) {
    if (result == "success") {
        _shortcuts.emplace(_data["shortcuts"]);
    } else if (_data.isMember("message")) {
        _message = _data["message"].asString();
    }
}

optional<KeyHelper::KeyCombination> SettingSyncServerMessage::shortcutManualCompletion() const {
    if (_shortcuts.has_value()) {
        if (const auto shortcutConfig = _shortcuts.value()["manualCompletion"];
            shortcutConfig.isMember("key") && shortcutConfig.isMember("modifiers")) {
            // return KeyHelper::KeyCombination{
            //     shortcutConfig["key"].get<int>(),
            //     shortcutConfig["modifiers"].get<int>()
            // };
            // TODO: Implement logic
        }
    }
    return nullopt;
}
