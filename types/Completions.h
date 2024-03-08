#pragma once

#include <string>
#include <vector>

namespace types {
    class Completions {
    public:
        const std::string actionId;

        Completions() = default;

        Completions(std::string actionId, const std::vector<std::string>& candidates);

        [[nodiscard]] uint32_t size() const;

        [[nodiscard]] std::tuple<std::string, uint32_t> current() const;

        [[nodiscard]] std::tuple<std::string, uint32_t> next();

        [[nodiscard]] std::tuple<std::string, uint32_t> previous();

    private:
        const std::vector<std::string> _candidates;
        uint32_t _currentIndex{};
    };
}
