#pragma once

#include <optional>
#include <nlohmann/json.hpp>

#include <types/keys.h>

namespace models {
    class CompletionConfig {
    public:
        const std::optional<std::chrono::milliseconds> debounceDelay;
        const std::optional<uint32_t> pasteFixMaxTriggerLineCount, prefixLineCount, recentFileCount, suffixLineCount;

        explicit CompletionConfig(const nlohmann::json& data);
    };

    class GenericConfig {
    public:
        const std::optional<std::chrono::seconds> autoSaveInterval;
        const std::optional<std::chrono::milliseconds> interactionUnlockDelay;

        explicit GenericConfig(const nlohmann::json& data);
    };

    class ShortcutConfig {
    public:
        const std::optional<types::KeyCombination> commit;
        const std::optional<types::KeyCombination> manualCompletion;

        explicit ShortcutConfig(const nlohmann::json& data);
    };

    class StatisticConfig {
    public:
        const std::optional<bool> checkEditedCompletion;

        explicit StatisticConfig(const nlohmann::json& data);
    };
}
