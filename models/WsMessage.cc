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

CompletionAcceptClientMessage::CompletionAcceptClientMessage(const string& completion)
    : WsMessage(WsAction::CompletionAccept, iconv::gbkToUtf8(completion)) {}

CompletionCacheClientMessage::CompletionCacheClientMessage(const bool isDelete)
    : WsMessage(WsAction::CompletionCache, isDelete) {}

CompletionCancelClientMessage::CompletionCancelClientMessage()
    : WsMessage(WsAction::CompletionCancel) {}

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
    for (const auto& [name, path, startLine, endLine]: symbols) {
        _data["symbols"].push_back({
            {"name", iconv::needEncode ? iconv::gbkToUtf8(name) : name},
            {"path", iconv::needEncode ? iconv::gbkToUtf8(path) : path},
            {"startLine", startLine},
            {"endLine", endLine},
        });
    }
}

CompletionSelectClientMessage::CompletionSelectClientMessage(
    const string& completion,
    const uint32_t currentIndex,
    const uint32_t totalCount,
    const int64_t xPos,
    const int64_t yPos
): WsMessage(
    WsAction::CompletionSelect, {
        {"completion", completion},
        {
            "count", {
                {"index", currentIndex},
                {"total", totalCount}
            }
        },
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
