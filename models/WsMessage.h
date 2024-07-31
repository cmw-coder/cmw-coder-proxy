#pragma once

#include <nlohmann/json.hpp>

#include <helpers/KeyHelper.h>
#include <models/ReviewReference.h>
#include <models/SymbolInfo.h>
#include <types/CaretPosition.h>
#include <types/Completions.h>
#include <types/Selection.h>
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

    class ChatInsertServerMessage final : public WsMessage {
    public:
        const std::string result;

        explicit ChatInsertServerMessage(nlohmann::json&& data);

        [[nodiscard]] std::string message() const;

        [[nodiscard]] std::optional<std::string> content() const;

    private:
        std::string _message;
        std::optional<std::string> _content{};
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

    class CompletionEditClientMessage final : public WsMessage {
    public:
        enum class KeptRatio {
            All,
            Few,
            Most,
            None,
        };

        explicit CompletionEditClientMessage(
            const std::string& actionId,
            uint32_t count,
            const std::string& editedContent,
            KeptRatio keptRatio
        );
    };

    class CompletionGenerateClientMessage final : public WsMessage {
    public:
        CompletionGenerateClientMessage(
            const types::CaretPosition& caret,
            const std::filesystem::path& path,
            const std::string& prefix,
            const std::vector<std::filesystem::path>& recentFiles,
            const std::string& suffix,
            const std::vector<SymbolInfo>& symbols,
            int64_t completionStartTime,
            int64_t symbolStartTime,
            int64_t completionEndTime
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

        explicit CompletionGenerateServerMessage(nlohmann::json&& data);

        [[nodiscard]] std::string message() const;

        [[nodiscard]] std::optional<types::Completions> completions() const;

    private:
        CompletionType _type{CompletionType::Snippet};
        std::string _message;
        std::optional<types::Completions> _completionsOpt{};
    };

    class CompletionSelectClientMessage final : public WsMessage {
    public:
        explicit CompletionSelectClientMessage(
            const std::string& actionId,
            uint32_t index,
            int64_t height,
            int64_t x,
            int64_t y
        );
    };

    class EditorCommitClientMessage final : public WsMessage {
    public:
        explicit EditorCommitClientMessage(const std::filesystem::path& path);
    };

    class EditorFocusStateClientMessage final : public WsMessage {
    public:
        explicit EditorFocusStateClientMessage(bool isFocused);
    };

    class EditorPasteClientMessage final : public WsMessage {
    public:
        explicit EditorPasteClientMessage(uint32_t count);
    };

    class EditorSwitchFileMessage final : public WsMessage {
    public:
        explicit EditorSwitchFileMessage(const std::filesystem::path& path);
    };

    class EditorSwitchProjectClientMessage final : public WsMessage {
    public:
        explicit EditorSwitchProjectClientMessage(const std::filesystem::path& path);
    };

    class EditorSelectionClientMessage final : public WsMessage {
    public:
        explicit EditorSelectionClientMessage(
            const std::filesystem::path& path,
            const std::string& content = {},
            const std::string& block = {},
            const types::Selection& selection = {},
            int64_t height = {},
            int64_t x = {},
            int64_t y = {}
        );
    };

    class HandShakeClientMessage final : public WsMessage {
    public:
        explicit HandShakeClientMessage(const std::filesystem::path& currentProject, std::string&& version);
    };

    class ReviewRequestClientMessage final : public WsMessage {
    public:
        explicit ReviewRequestClientMessage(
            const std::string& id,
            const std::vector<ReviewReference>& reviewReferences
        );
    };

    class ReviewRequestServerMessage final : public WsMessage {
    public:
        const std::string result;

        explicit ReviewRequestServerMessage(nlohmann::json&& data);

        [[nodiscard]] std::string content() const;

        [[nodiscard]] std::string id() const;

        [[nodiscard]] std::string message() const;

        [[nodiscard]] std::filesystem::path path() const;

        [[nodiscard]] types::Selection selection() const;

    private:
        std::filesystem::path _path;
        std::string _content, _id, _message;
        types::Selection _selection{};
    };

    class SettingSyncServerMessage final : public WsMessage {
    public:
        const std::string result;

        explicit SettingSyncServerMessage(nlohmann::json&& data);

        [[nodiscard]] std::string message() const;

        [[nodiscard]] std::optional<helpers::KeyHelper::KeyCombination> shortcutManualCompletion() const;

    private:
        std::string _message;
        std::optional<nlohmann::json> _shortcuts{};
    };
}
