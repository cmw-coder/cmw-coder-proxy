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

WsMessage::WsMessage(const WsAction action, nlohmann::json&& data)
    : action(action), _data(move(data)) {}

string WsMessage::parse() const {
    nlohmann::json jsonMessage = {{"action", enum_name(action)}};

    if (!_data.empty()) {
        jsonMessage["data"] = _data;
    }

    return jsonMessage.dump();
}

ChatInsertServerMessage::ChatInsertServerMessage(nlohmann::json&& data)
    : WsMessage(WsAction::CompletionGenerate, move(data)), result(_data["result"].get<string>()) {
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
            {"actionId", actionId},
            {"index", index},
        }
    ) {}

CompletionCacheClientMessage::CompletionCacheClientMessage(const bool isDelete)
    : WsMessage(WsAction::CompletionCache, isDelete) {}

CompletionCancelClientMessage::CompletionCancelClientMessage(const string& actionId, bool isExplicit)
    : WsMessage(
        WsAction::CompletionCancel, {
            {"actionId", actionId},
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
        {"actionId", actionId},
        {"count", count},
        {"editedContent", editedContent},
        {"ratio", enum_name(keptRatio)},
    }
) {}

CompletionGenerateClientMessage::CompletionGenerateClientMessage(
    const CaretPosition& caret,
    const filesystem::path& path,
    const string& prefix,
    const vector<filesystem::path>& recentFiles,
    const string& suffix,
    const vector<SymbolInfo>& symbols,
    uint64_t completionStartTime,
    uint64_t symbolStartTime,
    uint64_t completionEndTime
): WsMessage(
    WsAction::CompletionGenerate, {
        {
            "caret", {
                {"character", caret.character},
                {"line", caret.line},
            }
        },
        {"path", iconv::autoDecode(path.generic_string())},
        {"prefix", prefix},
        {"recentFiles", nlohmann::json::array()},
        {"suffix", suffix},
        {"symbols", nlohmann::json::array()},
        {
            "times", {
                "start", completionStartTime,
                "symbol", symbolStartTime,
                "end", completionEndTime,
            }
        }
    }
) {
    for (const auto& recentFile: recentFiles) {
        _data["recentFiles"].push_back(iconv::autoDecode(recentFile.generic_string()));
    }
    for (const auto& [path, name, type, startLine, endLine]: symbols) {
        _data["symbols"].push_back({
            {"endLine", endLine},
            {"name", name},
            {"path", iconv::autoDecode(path.generic_string())},
            {"startLine", startLine},
            {"type", enum_name(type)},
        });
    }
}

CompletionGenerateServerMessage::CompletionGenerateServerMessage(nlohmann::json&& data)
    : WsMessage(WsAction::CompletionGenerate, move(data)), result(_data["result"].get<string>()) {
    if (result == "success") {
        if (const auto completionTypeopt = enum_cast<CompletionType>(
            _data["completions"]["type"].get<string>()
        ); completionTypeopt.has_value()) {
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
        {"actionId", actionId},
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

EditorSelectionClientMessage::EditorSelectionClientMessage(
    const filesystem::path& path,
    const string& content,
    const string& block,
    const Selection& selection,
    int64_t height,
    int64_t x,
    int64_t y
) : WsMessage(
    WsAction::EditorSelection, {
        {"path", iconv::autoDecode(path.generic_string())},
        {"content", content},
        {"block", block},
        {
            "begin", {
                {"character", selection.begin.character},
                {"line", selection.begin.line},
            }
        },
        {
            "end", {
                {"character", selection.end.character},
                {"line", selection.end.line},
            }
        },
        {
            "dimensions", {
                {"height", height},
                {"x", x},
                {"y", y},
            }
        }
    }
) {}

EditorSwitchSvnClientMessage::EditorSwitchSvnClientMessage(const filesystem::path& path)
    : WsMessage(WsAction::EditorSwitchSvn, iconv::autoDecode(path.generic_string())) {}

HandShakeClientMessage::HandShakeClientMessage(const filesystem::path& currentProject, string&& version)
    : WsMessage(
        WsAction::HandShake, {
            {"pid", GetCurrentProcessId()},
            {"currentProject", iconv::autoDecode(currentProject.generic_string())},
            {"version", version},
        }
    ) {}

ReviewRequestClientMessage::ReviewRequestClientMessage(const vector<ReviewReference>& reviewReferences)
    : WsMessage(WsAction::ReviewRequest, nlohmann::json::array()) {
    for (const auto& [path, name, content, type, startLine, endLine, depth]: reviewReferences) {
        _data.push_back({
            {"name", name},
            {"type", enum_name(type)},
            {"content", content},
            {"depth", depth},
            {"path", iconv::autoDecode(path.generic_string())},
            {
                "range", {
                    {"startLine", startLine},
                    {"endLine", endLine},
                }
            },
        });
    }
}

ReviewRequestServerMessage::ReviewRequestServerMessage(nlohmann::json&& data)
    : WsMessage(WsAction::ReviewRequest, move(data)),
      result(_data["result"].get<string>()) {
    if (result == "success") {
        _path = iconv::toPath(_data["path"].get<string>());
        _content = _data["content"].get<string>();
        _selection = {
            {
                {},
                _data["beginLine"].get<uint32_t>(),
            },
            {
                {},
                _data["endLine"].get<uint32_t>(),
            }
        };
    } else if (_data.contains("message")) {
        _message = _data["message"].get<string>();
    }
}

string ReviewRequestServerMessage::content() const {
    return _content;
}

string ReviewRequestServerMessage::message() const {
    return _message;
}

filesystem::path ReviewRequestServerMessage::path() const {
    return _path;
}

Selection ReviewRequestServerMessage::selection() const {
    return _selection;
}

SettingSyncServerMessage::SettingSyncServerMessage(nlohmann::json&& data)
    : WsMessage(WsAction::SettingSync, move(data)),
      result(_data["result"].get<string>()) {
    if (result == "success") {
        _shortcuts.emplace(_data["shortcuts"]);
    } else if (_data.contains("message")) {
        _message = _data["message"].get<string>();
    }
}

string SettingSyncServerMessage::message() const {
    return _message;
}

optional<KeyHelper::KeyCombination> SettingSyncServerMessage::shortcutManualCompletion() const {
    if (_shortcuts.has_value()) {
        if (const auto shortcutConfig = _shortcuts.value()["manualCompletion"];
            shortcutConfig.contains("key") && shortcutConfig.contains("modifiers")) {
            // return KeyHelper::KeyCombination{
            //     shortcutConfig["key"].get<int>(),
            //     shortcutConfig["modifiers"].get<int>()
            // };
            // TODO: Implement logic
        }
    }
    return nullopt;
}
