//
// Created by particleg on 24-8-6.
//

#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include <readtags.h>

namespace types {
    class TagEntry {
    public:
        bool fileScope;
        std::string name, file, kind;

        struct {
            std::string pattern;
            unsigned long lineNumber;
        } address;

        std::unordered_map<std::string, std::string> fields;

        explicit TagEntry(const tagEntry& entry);

        [[nodiscard]] std::optional<uint32_t> getEndLine() const;

        [[nodiscard]] std::optional<std::pair<std::string, std::string>> getReferenceTarget() const;
    };
}
