#include <thread>
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

CompletionAcceptClientMessage::CompletionAcceptClientMessage(const string& completion)
    : WsMessage(WsAction::CompletionAccept, encode(completion, crypto::Encoding::Base64)) {}

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
        {"path", encode(path, crypto::Encoding::Base64)},
        {"prefix", encode(prefix, crypto::Encoding::Base64)},
        {"recentFiles", nlohmann::json::array()},
        {"suffix", suffix},
        {"symbols", nlohmann::json::array()},
    }
) {
    auto recentFilesList = thread([this, &recentFiles] {
        for (const auto& recentFile: recentFiles) {
            _data["recentFiles"].push_back(encode(recentFile, crypto::Encoding::Base64));
        }
    });

    auto symbolsList = thread([this, &symbols] {
        for (const auto& [name, path, startLine, endLine]: symbols) {
            _data["symbols"].push_back({
                {"name", encode(name, crypto::Encoding::Base64)},
                {"path", encode(path, crypto::Encoding::Base64)},
                {"startLine", startLine},
                {"endLine", endLine},
            });
        }
    });

    recentFilesList.join();
    symbolsList.join();
}

CompletionSelectClientMessage::CompletionSelectClientMessage(
    const string& completion,
    const uint32_t currentIndex,
    uint32_t totalCount,
    int64_t xPos,
    int64_t yPos
): WsMessage(
    WsAction::CompletionSelect, {
        {"completion", encode(completion, crypto::Encoding::Base64)},
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
