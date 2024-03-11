#include <magic_enum.hpp>
#include <models/WsMessage.h>
#include <utils/iconv.h>

#include <windows.h>

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

CompletionGenerateClientMessage::CompletionGenerateClientMessage(
    const CaretPosition& caret,
    const string& path,
    const string& prefix,
    const string& project,
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
        {"project", iconv::needEncode ? iconv::gbkToUtf8(project) : project},
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
      result(_data["result"].get<string>()),
      type(enum_cast<CompletionType>(_data["completions"]["type"].get<string>()).value_or(CompletionType::Snippet)) {
    if (result == "success") {
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

CompletionKeptClientMessage::CompletionKeptClientMessage(
    const string& actionId,
    const uint32_t count,
    const Ratio ratio
): WsMessage(
    WsAction::CompletionKept, {
        {"actionId", actionId},
        {"count", count},
        {"ratio", enum_name(ratio)},
    }
) {}

CompletionSelectClientMessage::CompletionSelectClientMessage(
    const string& actionId,
    uint32_t index,
    int64_t xPos,
    int64_t yPos
): WsMessage(
    WsAction::CompletionSelect, {
        {"actionId", actionId},
        {"index", index},
        {
            "position", {
                {"x", xPos},
                {"y", yPos}
            }
        }
    }
) {}

DebugSyncClientMessage::DebugSyncClientMessage(const string& content, const string& path)
    : WsMessage(
        WsAction::DebugSync, {
            {"content", iconv::needEncode ? iconv::gbkToUtf8(content) : content},
            {"path", iconv::needEncode ? iconv::gbkToUtf8(path) : path}
        }
    ) {}

EditorFocusStateClientMessage::EditorFocusStateClientMessage(const bool isFocused)
    : WsMessage(WsAction::EditorFocusState, isFocused) {}

EditorSwitchProjectClientMessage::EditorSwitchProjectClientMessage(const string& path)
    : WsMessage(WsAction::EditorSwitchProject, iconv::needEncode ? iconv::gbkToUtf8(path) : path) {}

HandShakeClientMessage::HandShakeClientMessage(string&& version)
    : WsMessage(
        WsAction::HandShake, {
            {"pid", GetCurrentProcessId()},
            {"version", version},
        }
    ) {}
