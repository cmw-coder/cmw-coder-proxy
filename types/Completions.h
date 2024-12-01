#pragma once

#include <string>
#include <vector>

#include <types/CompletionComponents.h>
#include <types/Selection.h>

namespace types {
    class Completions {
    public:
        const std::string actionId;
        const CompletionComponents::GenerateType generateType;

        Completions(
            std::string actionId,
            CompletionComponents::GenerateType generateType,
            const std::vector<std::pair<std::string, Selection>>& candidates
        );

        [[nodiscard]] std::tuple<std::string, Selection, uint32_t> current() const;

        [[nodiscard]] bool empty() const;

        [[nodiscard]] std::tuple<std::string, Selection, uint32_t> next();

        [[nodiscard]] std::tuple<std::string, Selection, uint32_t> previous();

    private:
        const std::vector<std::pair<std::string, Selection>> _candidates;
        uint32_t _currentIndex{};
    };
}
