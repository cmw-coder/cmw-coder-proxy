#include <magic_enum/magic_enum.hpp>

#include <models/WsMessage.h>
#include <utils/common.h>
#include <utils/iconv.h>

#include <windows.h>

using namespace magic_enum;
using namespace models;
using namespace std;
using namespace types;
using namespace utils;

WsMessage::WsMessage(const WsAction action): id(common::uuid()), action(action) {}

WsMessage::WsMessage(const WsAction action, nlohmann::json&& data)
    : id(common::uuid()), action(action), _data(move(data)) {}

string WsMessage::parse() const {
    nlohmann::json jsonMessage = {
        {"id", id},
        {"action", enum_name(action)},
    };

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

CompletionGenerateClientMessage::CompletionGenerateClientMessage(const CompletionComponents& completionComponents)
    : WsMessage(WsAction::CompletionGenerate, completionComponents.toJson()) {}

CompletionGenerateServerMessage::CompletionGenerateServerMessage(nlohmann::json&& data)
    : WsMessage(WsAction::CompletionGenerate, move(data)), result(_data["result"].get<string>()) {
    if (result == "success") {
        if (const auto completionTypeOpt = enum_cast<CompletionType>(
            _data["completions"]["type"].get<string>()
        ); completionTypeOpt.has_value()) {
            _type = completionTypeOpt.value();
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

EditorPasteClientMessage::EditorPasteClientMessage(
    const string& content,
    const CaretPosition& caretPosition,
    const vector<filesystem::path>& recentFiles
): WsMessage(
    WsAction::EditorPaste, {
        {"content", content},
        {
            "position", {
                {"character", caretPosition.character},
                {"line", caretPosition.line},
            }
        },
        {"recentFiles", nlohmann::json::array()},
    }
) {
    for (const auto& recentFile: recentFiles) {
        _data["recentFiles"].push_back(iconv::autoDecode(recentFile.generic_string()));
    }
}

EditorPasteServerMessage::EditorPasteServerMessage(nlohmann::json&& data)
    : CompletionGenerateServerMessage(move(data)) {}

EditorSwitchFileMessage::EditorSwitchFileMessage(const filesystem::path& path)
    : WsMessage(WsAction::EditorSwitchFile, iconv::autoDecode(path.generic_string())) {}

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

HandShakeClientMessage::HandShakeClientMessage(const filesystem::path& currentProject, string&& version)
    : WsMessage(
        WsAction::HandShake, {
            {"pid", GetCurrentProcessId()},
            {"currentProject", iconv::autoDecode(currentProject.generic_string())},
            {"version", version},
        }
    ) {}

ReviewRequestClientMessage::ReviewRequestClientMessage(
    const string& id,
    const vector<ReviewReference>& reviewReferences
)
    : WsMessage(
        WsAction::ReviewRequest, {
            {"id", id},
            {"references", nlohmann::json::array()}
        }
    ) {
    for (const auto& [path, name, content, type, startLine, endLine, depth]: reviewReferences) {
        _data["references"].push_back({
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
        _id = _data["id"].get<string>();
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

string ReviewRequestServerMessage::id() const {
    return _id;
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
    : WsMessage(WsAction::SettingSync, move(data)), result(_data["result"].get<string>()) {
    if (result == "success") {
        if (_data.contains("completionConfig")) {
            _completionConfig.emplace(CompletionConfig(_data["completionConfig"]));
        }
        if (_data.contains("shortcutConfig")) {
            _shortcutConfig.emplace(ShortcutConfig(_data["shortcutConfig"]));
        }
    } else if (_data.contains("message")) {
        _message = _data["message"].get<string>();
    }
}

string SettingSyncServerMessage::message() const {
    return _message;
}

optional<CompletionConfig> SettingSyncServerMessage::completionConfig() const {
    return _completionConfig;
}

optional<ShortcutConfig> SettingSyncServerMessage::shortcutConfig() const {
    return _shortcutConfig;
}
