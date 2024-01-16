#include <types/WsMessage.h>
#include <utils/crypto.h>

#include <windows.h>

using namespace std;
using namespace types;
using namespace utils;

WsMessage::WsMessage(const WsAction action): action(action) {}

WsMessage::WsMessage(const WsAction action, nlohmann::json&& data): action(action), _data(move(data)) {}

string WsMessage::parse() const {
    if (_data.empty()) {
        return nlohmann::json{
            {"action", action},
        }.dump();
    }
    return nlohmann::json{
        {"action", action},
        {"data", _data}
    }.dump();
}

CompletionAcceptClientMessage::CompletionAcceptClientMessage(string&& completion)
    : WsMessage(WsAction::CompletionAccept, move(completion)) {}

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
            "count",
            {"index", currentIndex},
            {"total", totalCount}
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
            {"content", encode(content, crypto::Encoding::Base64)},
            {"path", encode(path, crypto::Encoding::Base64)}
        }
    ) {}

EditorFocusStateClientMessage::EditorFocusStateClientMessage(bool isFocused)
    : WsMessage(WsAction::EditorFocusState, isFocused) {}

EditorSwitchProjectClientMessage::EditorSwitchProjectClientMessage(const string& path)
    : WsMessage(WsAction::EditorSwitchProject, encode(path, crypto::Encoding::Base64)) {}

HandShakeClientMessage::HandShakeClientMessage(string&& version)
    : WsMessage(
        WsAction::HandShake, {
            {"pid", GetCurrentProcessId()},
            {"version", version},
        }
    ) {}
