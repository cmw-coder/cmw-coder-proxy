#include <magic_enum.hpp>

#include <models/configs.h>

using namespace magic_enum;
using namespace models;
using namespace std;
using namespace types;

namespace {
    KeyCombination parseShortcutConfig(const nlohmann::json& shortcutConfig) {
        ModifierSet modifiers;
        for (const auto& modifierString: shortcutConfig["modifiers"]) {
            if (const auto modifierOpt = enum_cast<Modifier>(modifierString.get<int>());
                modifierOpt.has_value()) {
                modifiers.insert(modifierOpt.value());
            }
        }
        return {shortcutConfig["keycode"].get<uint32_t>(), modifiers};
    }
}

CompletionConfig::CompletionConfig(const nlohmann::json& data)
    : debounceDelay(
          data.contains("debounceDelay") ? optional(data["debounceDelay"].get<uint32_t>()) : nullopt
      ),
      prefixLineCount(
          data.contains("prefixLineCount") ? optional(data["prefixLineCount"].get<uint32_t>()) : nullopt
      ),
      suffixLineCount(
          data.contains("suffixLineCount") ? optional(data["suffixLineCount"].get<uint32_t>()) : nullopt
      ) {}

CompletionConfig::CompletionConfig(uint32_t debounceDelay, uint32_t prefixLineCount, uint32_t suffixLineCount)
    : debounceDelay(debounceDelay), prefixLineCount(prefixLineCount), suffixLineCount(suffixLineCount) {}

ShortcutConfig::ShortcutConfig(const nlohmann::json& data)
    : commit(
          data.contains("commit") ? optional(parseShortcutConfig(data["commit"])) : nullopt
      ),
      manualCompletion(
          data.contains("manualCompletion") ? optional(parseShortcutConfig(data["manualCompletion"])) : nullopt
      ) {}

ShortcutConfig::ShortcutConfig(const KeyCombination& commit, const KeyCombination& manualCompletion)
    : commit(commit), manualCompletion(manualCompletion) {}
