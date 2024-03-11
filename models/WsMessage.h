#pragma once

#include <nlohmann/json.hpp>

#include <models/SymbolInfo.h>
#include <types/CaretPosition.h>
#include <types/Completions.h>
#include <types/WsAction.h>

namespace models {
    class WsMessage {
    public:
        const types::WsAction action;

        [[nodiscard]] std::string parse() const;

    protected:
        nlohmann::json _data;

        explicit WsMessage(types::WsAction action);

        WsMessage(types::WsAction action, nlohmann::json&& data);

        virtual ~WsMessage() = default;
    };

    class CompletionAcceptClientMessage final : public WsMessage {
    public:
        explicit CompletionAcceptClientMessage(const std::string& actionId, uint32_t index);
    };

    class CompletionCacheClientMessage final : public WsMessage {
    public:
        explicit CompletionCacheClientMessage(bool isDelete);
    };

    class CompletionCancelClientMessage final : public WsMessage {
    public:
        explicit CompletionCancelClientMessage(const std::string& actionId, bool isExplicit);
    };

    class CompletionGenerateClientMessage final : public WsMessage {
    public:
        CompletionGenerateClientMessage(
            const types::CaretPosition& caret,
            const std::string& path,
            const std::string& prefix,
            const std::string& project,
            const std::vector<std::string>& recentFiles,
            const std::string& suffix,
            const std::vector<SymbolInfo>& symbols
        );
    };

    class CompletionGenerateServerMessage final : public WsMessage {
    public:
        enum class CompletionType {
            Function,
            Line,
            Snippet,
        };

        const std::string result;
        const CompletionType type;

        explicit CompletionGenerateServerMessage(nlohmann::json&& data);

        [[nodiscard]] std::string message() const;

        [[nodiscard]] std::optional<types::Completions> completions() const;

    private:
        std::string _message;
        std::optional<types::Completions> _completionsOpt{};
    };

    class CompletionKeptClientMessage final : public WsMessage {
    public:
        enum class Ratio {
            All,
            Most,
            Few,
        };

        explicit CompletionKeptClientMessage(
            const std::string& actionId,
            uint32_t count,
            Ratio ratio
        );
    };

    class CompletionSelectClientMessage final : public WsMessage {
    public:
        explicit CompletionSelectClientMessage(
            const std::string& actionId,
            uint32_t index,
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
