#pragma once

#include <optional>
#include <shared_mutex>

namespace types {
    class CompletionCache {
    public:
        std::optional<std::pair<char, std::optional<std::string>>> previous();

        std::optional<std::pair<char, std::optional<std::string>>> next();

        std::pair<std::string, int64_t> reset(std::string content = {});

        [[nodiscard]] bool valid() const;

    private:
        std::string _content;
        int64_t _index = -1;
    };
}
