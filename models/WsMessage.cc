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

WsMessage::WsMessage(const WsAction action, nlohmann::json&& data): action(action), _data(move(data)) {}

string WsMessage::parse() const {
    nlohmann::json jsonMessage = {{"action", enum_name(action)}};

    if (!_data.empty()) {
        jsonMessage["data"] = _data;
    }

    return jsonMessage.dump();
}

ChatInsertServerMessage::ChatInsertServerMessage(nlohmann::json&& data)
    : WsMessage(WsAction::CompletionGenerate, move(data)),
      result(_data["result"].get<string>()) {
    if (result == "success") {
        _content.emplace(_data["content"].get<string>());
    } else if (_data.contains("message")) {
        _message = _data["message"].get<string>();
    }
}

optional<string> ChatInsertServerMessage::content() const {
    return _content;
}

CompletionAcceptClientMessage::CompletionAcceptClientMessage(const string& actionId, uint32_t index)
    : WsMessage(
        WsAction::CompletionAccept, {
            {"actionId", iconv::needEncode ? iconv::gbkToUtf8(actionId) : actionId},
            {"index", index},
        }
    ) {}

CompletionCacheClientMessage::CompletionCacheClientMessage(const bool isDelete)
    : WsMessage(WsAction::CompletionCache, isDelete) {}

CompletionCancelClientMessage::CompletionCancelClientMessage(const string& actionId, bool isExplicit)
    : WsMessage(
        WsAction::CompletionCancel, {
            {"actionId", iconv::needEncode ? iconv::gbkToUtf8(actionId) : actionId},
            {"explicit", isExplicit}
        }
    ) {}

CompletionEditClientMessage::CompletionEditClientMessage(
    const string& actionId,
    const uint32_t count,
    const string& editedContent,
    const KeptRatio keptRatio
): WsMessage(
    WsAction::CompletionEdit, {
        {"actionId", iconv::needEncode ? iconv::gbkToUtf8(actionId) : actionId},
        {"count", count},
        {"editedContent", iconv::needEncode ? iconv::gbkToUtf8(editedContent) : editedContent},
        {"ratio", enum_name(keptRatio)},
    }
) {}

CompletionGenerateClientMessage::CompletionGenerateClientMessage(
    const CaretPosition& caret,
    const string& path,
    const string& prefix,
    const vector<string>& recentFiles,
    const string& suffix,
    const vector<SymbolInfo>& symbols
): WsMessage(
    WsAction::CompletionGenerate, {
        {
            "caret", {
                {"character", caret.character},
                {"line", caret.line},
            }
        },
        {"path", iconv::needEncode ? iconv::gbkToUtf8(path) : path},
        {"prefix", iconv::needEncode ? iconv::gbkToUtf8(prefix) : prefix},
        {"recentFiles", nlohmann::json::array()},
        {"suffix", iconv::needEncode ? iconv::gbkToUtf8(suffix) : suffix},
        {"symbols", nlohmann::json::array()},
    }
) {
    for (const auto& recentFile: recentFiles) {
        _data["recentFiles"].push_back(iconv::needEncode ? iconv::gbkToUtf8(recentFile) : recentFile);
    }
    for (const auto& [name, path, type, startLine, endLine]: symbols) {
        _data["symbols"].push_back({
            {"endLine", endLine},
            {"name", iconv::needEncode ? iconv::gbkToUtf8(name) : name},
            {"path", iconv::needEncode ? iconv::gbkToUtf8(path) : path},
            {"startLine", startLine},
            {"type", iconv::needEncode ? iconv::gbkToUtf8(type) : type},
        });
    }
}

CompletionGenerateServerMessage::CompletionGenerateServerMessage(nlohmann::json&& data)
    : WsMessage(WsAction::CompletionGenerate, move(data)),
      result(_data["result"].get<string>()) {
    if (result == "success") {
        if (const auto completionTypeopt = enum_cast<CompletionType>(_data["completions"]["type"].get<string>());
            completionTypeopt.has_value()) {
            _type = completionTypeopt.value();
        }
        _completionsOpt.emplace(
            _data["actionId"].get<string>(),
            _data["completions"]["candidates"].get<vector<string>>()
        );
    } else if (_data.contains("message")) {
        _message = _data["message"].get<string>();
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
): WsMessage(
    WsAction::CompletionSelect, {
        {"actionId", iconv::needEncode ? iconv::gbkToUtf8(actionId) : actionId},
        {"index", index},
        {
            "dimensions", {
                {"height", height},
                {"x", x},
                {"y", y},
            }
        }
    }
) {}

DebugSyncClientMessage::DebugSyncClientMessage(const string& content, const string& path): WsMessage(
    WsAction::DebugSync, {
        {"content", iconv::needEncode ? iconv::gbkToUtf8(content) : content},
        {"path", iconv::needEncode ? iconv::gbkToUtf8(path) : path}
    }
) {}

EditorFocusStateClientMessage::EditorFocusStateClientMessage(const bool isFocused)
    : WsMessage(WsAction::EditorFocusState, isFocused) {}

EditorPasteClientMessage::EditorPasteClientMessage(const uint32_t count): WsMessage(
    WsAction::EditorPaste, {
        {"count", count},
    }
) {}

EditorSwitchProjectClientMessage::EditorSwitchProjectClientMessage(const string& path)
    : WsMessage(WsAction::EditorSwitchProject, iconv::needEncode ? iconv::gbkToUtf8(path) : path) {}

HandShakeClientMessage::HandShakeClientMessage(string&& currentProject, string&& version)
    : WsMessage(
        WsAction::HandShake, {
            {"pid", GetCurrentProcessId()},
            {"currentProject", iconv::needEncode ? iconv::gbkToUtf8(currentProject) : currentProject},
            {"version", iconv::needEncode ? iconv::gbkToUtf8(version) : version},
        }
    ) {}

SettingSyncServerMessage::SettingSyncServerMessage(nlohmann::json&& data)
    : WsMessage(WsAction::SettingSync, move(data)),
      result(_data["result"].get<string>()) {
    if (result == "success") {
        _shortcuts.emplace(_data["shortcuts"]);
    } else if (_data.contains("message")) {
        _message = _data["message"].get<string>();
    }
}

optional<KeyHelper::KeyCombination> SettingSyncServerMessage::shortcutManualCompletion() const {
    if (_shortcuts.has_value()) {
        const auto shortcutConfig = _shortcuts.value()["manualCompletion"];
        if (shortcutConfig.contains("key") && shortcutConfig.contains("modifiers")) {
            // return KeyHelper::KeyCombination{
            //     shortcutConfig["key"].get<int>(),
            //     shortcutConfig["modifiers"].get<int>()
            // };
            // TODO: Implement logic
        }
    }
    return nullopt;
}
