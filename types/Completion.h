#pragma once

#include <string>

namespace types {
    class Completion {
    public:
        Completion(bool isSnippet, std::string content);

        [[nodiscard]] bool isSnippet() const;

        [[nodiscard]] const std::string &content() const;

        [[nodiscard]] std::string stringify() const;

    private:
        const bool _isSnippet;
        const std::string _content;
    };
}
