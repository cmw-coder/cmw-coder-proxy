#pragma once

#include <optional>

#include <types/CompletionComponents.h>
#include <types/Selection.h>

namespace types {
    class CompletionCache {
    public:
        std::optional<std::pair<char, std::optional<std::string>>> previous();

        std::optional<std::pair<char, std::optional<std::string>>> next();

        std::tuple<CompletionComponents::GenerateType, std::string, Selection, int64_t> reset(
            CompletionComponents::GenerateType generateType = CompletionComponents::GenerateType::Common,
            std::string content = {},
            Selection selection = {}
        );

        [[nodiscard]] bool valid() const;

    private:
        CompletionComponents::GenerateType _generateType = CompletionComponents::GenerateType::Common;
        std::string _content;
        Selection _selection{};
        int64_t _index = -1;
    };
}
