#pragma once

#include <nlohmann/json.hpp>

#include <types/CaretPosition.h>
#include <types/SymbolInfo.h>

namespace types {
    enum class WsAction {
        CompletionAccept,
        CompletionCache,
        CompletionCancel,
        CompletionGenerate,
        CompletionSelect,
        DebugSync,
        EditorFocusState,
        EditorSwitchProject,
        HandShake
    };

    class WsMessage {
    public:
        const WsAction action;

        [[nodiscard]] std::string parse() const;

    protected:
        nlohmann::json _data;

        explicit WsMessage(WsAction action);

        WsMessage(WsAction action, nlohmann::json&& data);

        virtual ~WsMessage() = default;
    };

    class CompletionAcceptClientMessage final : public WsMessage {
    public:
        explicit CompletionAcceptClientMessage(const std::string& completion);
    };

    class CompletionCacheClientMessage final : public WsMessage {
    public:
        explicit CompletionCacheClientMessage(bool isDelete);
    };

    class CompletionCancelClientMessage final : public WsMessage {
    public:
        CompletionCancelClientMessage();
    };

    class CompletionGenerateClientMessage final : public WsMessage {
    public:
        CompletionGenerateClientMessage(
            const CaretPosition& caret,
            const std::string& path,
            const std::string& prefix,
            const std::vector<std::string>& recentFiles,
            const std::string& suffix,
            const std::vector<SymbolInfo>& symbols
        );
    };

    class CompletionSelectClientMessage final : public WsMessage {
    public:
        explicit CompletionSelectClientMessage(
            const std::string& completion,
            uint32_t currentIndex,
            uint32_t totalCount,
            int64_t xPos,
            int64_t yPos
        );
    };

    class DebugSyncClientMessage final : public WsMessage {
    public:
        DebugSyncClientMessage(const std::string& content, const std::string& path);
    };

    class EditorFocusStateClientMessage final : public WsMessage {
    public:
        explicit EditorFocusStateClientMessage(bool isFocused);
    };

    class EditorSwitchProjectClientMessage final : public WsMessage {
    public:
        explicit EditorSwitchProjectClientMessage(const std::string& path);
    };

    class HandShakeClientMessage final : public WsMessage {
    public:
        explicit HandShakeClientMessage(std::string&& version);
    };
}
