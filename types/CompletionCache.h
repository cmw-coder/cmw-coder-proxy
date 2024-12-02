#pragma once

#include <optional>

#include <types/CompletionComponents.h>
#include <types/Selection.h>

namespace types {
    class CompletionCache {
    public:
        std::optional<std::pair<char, std::optional<std::string>>> previous();

        std::optional<std::pair<char, std::optional<std::string>>> next();

        std::tuple<std::string, int64_t> reset(std::string content = {});

        [[nodiscard]] bool valid() const;

    private:
        std::string _content;
        int64_t _index = -1;
    };
}
