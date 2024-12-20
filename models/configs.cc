#include <magic_enum/magic_enum.hpp>

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
    : completionOnPaste(
          data.contains("completionOnPaste") ? optional(data["completionOnPaste"].get<bool>()) : nullopt
      ),
      debounceDelay(
          data.contains("debounceDelay") ? optional(data["debounceDelay"].get<uint32_t>()) : nullopt
      ),
      interactionUnlockDelay(
          data.contains("interactionUnlockDelay") ? optional(data["interactionUnlockDelay"].get<uint32_t>()) : nullopt
      ),
      pasteMaxLineCount(
          data.contains("pasteMaxLineCount") ? optional(data["pasteMaxLineCount"].get<uint32_t>()) : nullopt
      ),
      prefixLineCount(
          data.contains("prefixLineCount") ? optional(data["prefixLineCount"].get<uint32_t>()) : nullopt
      ),
      recentFileCount(
          data.contains("recentFileCount") ? optional(data["recentFileCount"].get<uint32_t>()) : nullopt
      ),
      suffixLineCount(
          data.contains("suffixLineCount") ? optional(data["suffixLineCount"].get<uint32_t>()) : nullopt
      ) {}

ShortcutConfig::ShortcutConfig(const nlohmann::json& data)
    : commit(
          data.contains("commit") ? optional(parseShortcutConfig(data["commit"])) : nullopt
      ),
      manualCompletion(
          data.contains("manualCompletion") ? optional(parseShortcutConfig(data["manualCompletion"])) : nullopt
      ) {}
