#pragma once

#include <optional>
#include <nlohmann/json.hpp>

#include <types/keys.h>

namespace models {
    class CompletionConfig {
    public:
        const std::optional<uint32_t> debounceDelay;
        const std::optional<uint32_t> prefixLineCount;
        const std::optional<uint32_t> suffixLineCount;

        explicit CompletionConfig(const nlohmann::json& data);

        CompletionConfig(uint32_t debounceDelay, uint32_t prefixLineCount, uint32_t suffixLineCount);
    };

    class ShortcutConfig {
    public:
        const std::optional<types::KeyCombination> commit;
        const std::optional<types::KeyCombination> manualCompletion;

        explicit ShortcutConfig(const nlohmann::json& data);

        ShortcutConfig(const types::KeyCombination& commit, const types::KeyCombination& manualCompletion);
    };
}
