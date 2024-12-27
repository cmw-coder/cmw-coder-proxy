#include <magic_enum/magic_enum.hpp>

#include <models/WsMessage.h>
#include <utils/base64.h>
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
        auto type = CompletionComponents::GenerateType::Common;
        if (const auto completionTypeOpt = enum_cast<CompletionComponents::GenerateType>(
            _data["type"].get<string>()
        ); completionTypeOpt.has_value()) {
            type = completionTypeOpt.value();
        }
        _completionsOpt.emplace(
            _data["actionId"].get<string>(),
            type,
            Selection{
                {
                    _data["selection"]["begin"]["character"].get<uint32_t>(),
                    _data["selection"]["begin"]["line"].get<uint32_t>()
                },
                {
                    _data["selection"]["end"]["character"].get<uint32_t>(),
                    _data["selection"]["end"]["line"].get<uint32_t>()
                },
            },
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
    const CompletionComponents::GenerateType generateType,
    const uint32_t index,
    const int64_t height,
    const int64_t x,
    const int64_t y
): WsMessage(
    WsAction::CompletionSelect, {
        {"actionId", actionId},
        {"type", enum_name(generateType)},
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

EditorConfigServerMessage::EditorConfigServerMessage(nlohmann::json&& data)
    : WsMessage(WsAction::EditorConfig, move(data)), result(_data["result"].get<string>()) {
    if (result == "success") {
        if (_data.contains("completion")) {
            _completionConfig.emplace(CompletionConfig(_data["completion"]));
        }
        if (_data.contains("generic")) {
            _genericConfig.emplace(GenericConfig(_data["generic"]));
        }
        if (_data.contains("shortcut")) {
            _shortcutConfig.emplace(ShortcutConfig(_data["shortcut"]));
        }
        if (_data.contains("statistic")) {
            _statisticConfig.emplace(StatisticConfig(_data["statistic"]));
        }
    } else if (_data.contains("message")) {
        _message = _data["message"].get<string>();
    }
}

string EditorConfigServerMessage::message() const {
    return _message;
}

optional<CompletionConfig> EditorConfigServerMessage::completionConfig() const {
    return _completionConfig;
}

std::optional<GenericConfig> EditorConfigServerMessage::genericConfig() const {
    return _genericConfig;
}

optional<ShortcutConfig> EditorConfigServerMessage::shortcutConfig() const {
    return _shortcutConfig;
}

std::optional<StatisticConfig> EditorConfigServerMessage::statisticConfig() const {
    return _statisticConfig;
}

EditorPasteClientMessage::EditorPasteClientMessage(
    const CaretPosition& caretPosition,
    const string& infix,
    const string& prefix,
    const string& suffix,
    const vector<filesystem::path>& recentFiles
): WsMessage(
    WsAction::EditorPaste, {
        {
            "caret", {
                {"character", caretPosition.character},
                {"line", caretPosition.line},
            }
        },
        {
            "context", {
                {"infix", base64::to_base64(infix)},
                {"prefix", base64::to_base64(prefix)},
                {"suffix", base64::to_base64(suffix)},
            },
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

EditorStateClientMessage::EditorStateClientMessage(const bool isFocused)
    : WsMessage(WsAction::EditorState, {{"isFocused", isFocused}}) {}

EditorStateClientMessage::EditorStateClientMessage(const _Dimensions& dimensions)
    : WsMessage(
        WsAction::EditorState, {
            {
                "dimensions", {
                    {"height", dimensions.height},
                    {"width", dimensions.width},
                    {"x", dimensions.x},
                    {"y", dimensions.y},
                }
            }
        }
    ) {}

void EditorStateClientMessage::setFocused(bool isFocused) {
    _data["isFocused"] = isFocused;
}

void EditorStateClientMessage::setDimensions(const _Dimensions& dimensions) {
    _data["dimensions"] = {
        {"height", dimensions.height},
        {"width", dimensions.width},
        {"x", dimensions.x},
        {"y", dimensions.y},
    };
}

EditorSwitchFileMessage::EditorSwitchFileMessage(const filesystem::path& path)
    : WsMessage(WsAction::EditorSwitchFile, iconv::autoDecode(path.generic_string())) {}

EditorSwitchProjectClientMessage::EditorSwitchProjectClientMessage(const filesystem::path& path)
    : WsMessage(WsAction::EditorSwitchProject, iconv::autoDecode(path.generic_string())) {}

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
