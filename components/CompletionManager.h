#pragma once

#include <any>

#include <singleton_dclp.hpp>

#include <helpers/HttpHelper.h>
#include <models/SymbolInfo.h>
#include <types/CaretPosition.h>
#include <types/common.h>
#include <types/Completions.h>
#include <types/CompletionCache.h>

namespace components {
    class CompletionManager : public SingletonDclp<CompletionManager> {
    public:
        class AcceptedCompletion {
        public:
            std::string actionId;
            types::Time acceptedTime = std::chrono::high_resolution_clock::now();

            AcceptedCompletion(std::string actionId, std::string content, uint32_t line);

            [[nodiscard]] bool canReport() const;

            void addLine(uint32_t line);

            void removeLine(uint32_t line);

            [[nodiscard]] uint32_t getKeptLineCount() const;

        private:
            std::string _content;
            std::vector<uint32_t> _references;
        };

        struct Components {
            types::CaretPosition caretPosition;
            std::string path;
            std::string prefix;
            std::string project;
            std::vector<std::string> recentFiles;
            std::string suffix;
            std::vector<models::SymbolInfo> symbols;
        };

        CompletionManager();

        ~CompletionManager() override;

        void interactionCompletionAccept(const std::any&, bool& needBlockMessage);

        void interactionCompletionCancel(const std::any& data, bool&);

        void interactionDeleteInput(const std::any&, bool&);

        void interactionEnterInput(const std::any&, bool&);

        void interactionNavigateWithKey(const std::any& data, bool&);

        void interactionNavigateWithMouse(const std::any& data, bool&);

        void interactionNormalInput(const std::any& data, bool&);

        void interactionPaste(const std::any&, bool&);

        void interactionSave(const std::any&, bool&);

        void interactionUndo(const std::any&, bool&);

        void wsActionCompletionGenerate(nlohmann::json&& data);

        // TODO: Remove old methods

        void setAutoCompletion(bool isAutoCompletion);

    private:
        mutable std::shared_mutex _acceptedCompletionsMutex, _completionsMutex, _completionCacheMutex, _componentsMutex;
        Components _components;
        std::atomic<bool> _isAutoCompletion{true}, _isNewLine{true}, _isRunning{true},
                _needDiscardWsAction{false}, _needRetrieveCompletion{false};
        std::atomic<types::Time> _debounceRetrieveCompletionTime;
        std::optional<types::Completions> _completionsOpt;
        std::vector<AcceptedCompletion> _acceptedCompletions;
        types::CompletionCache _completionCache;

        void _cancelCompletion();

        bool _hasValidCache() const;

        void _prolongRetrieveCompletion();

        void _updateNeedRetrieveCompletion(bool need = true, char character = 0);

        void _sendCompletionGenerate();

        void _threadCheckAcceptedCompletions();

        void _threadDebounceRetrieveCompletion();
    };
}
