#include <magic_enum.hpp>
#include <types/WsMessage.h>

#include <windows.h>

using namespace magic_enum;
using namespace std;
using namespace types;

WsMessage::WsMessage(const WsAction action): action(action) {}

WsMessage::WsMessage(const WsAction action, nlohmann::json&& data): action(action), _data(move(data)) {}

string WsMessage::parse() const {
    if (_data.empty()) {
        return nlohmann::json{
            {"action", enum_name(action)},
        }.dump();
    }
    return nlohmann::json{
        {"action", enum_name(action)},
        {"data", _data}
    }.dump();
}

CompletionAcceptClientMessage::CompletionAcceptClientMessage(const string& completion)
    : WsMessage(WsAction::CompletionAccept, completion) {}

CompletionCacheClientMessage::CompletionCacheClientMessage(bool isDelete)
    : WsMessage(WsAction::CompletionCache, isDelete) {}

CompletionCancelClientMessage::CompletionCancelClientMessage()
    : WsMessage(WsAction::CompletionCancel) {}

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
        {"path", path},
        {"prefix", prefix},
        {"recentFiles", recentFiles},
        {"suffix", suffix},
        {"symbols", nlohmann::json::array()},
    }
) {
    for (const auto& [name, path, startLine, endLine]: symbols) {
        _data["symbols"].push_back({
            {"name", name},
            {"path", path},
            {"startLine", startLine},
            {"endLine", endLine},
        });
    }
}

CompletionSelectClientMessage::CompletionSelectClientMessage(
    const string& completion,
    const uint32_t currentIndex,
    uint32_t totalCount,
    int64_t xPos,
    int64_t yPos
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
                "x", xPos,
                "y", yPos
            }
        }
    }
) {}

DebugSyncClientMessage::DebugSyncClientMessage(const string& content, const string& path)
    : WsMessage(
        WsAction::DebugSync, {
            {"content", content},
            {"path", path}
        }
    ) {}

EditorFocusStateClientMessage::EditorFocusStateClientMessage(bool isFocused)
    : WsMessage(WsAction::EditorFocusState, isFocused) {}

EditorSwitchProjectClientMessage::EditorSwitchProjectClientMessage(const string& path)
    : WsMessage(WsAction::EditorSwitchProject, path) {}

HandShakeClientMessage::HandShakeClientMessage(string&& version)
    : WsMessage(
        WsAction::HandShake, {
            {"pid", GetCurrentProcessId()},
            {"version", version},
        }
    ) {}
