#pragma once

#include <optional>
#include <shared_mutex>
#include <string>

namespace types {
    class CompletionCache {
    public:
        class Completion {
        public:
            Completion(bool isSnippet, std::string content) : _isSnippet(isSnippet), _content(std::move(content)) {}

            [[nodiscard]] bool isSnippet() const {
                return _isSnippet;
            }

            [[nodiscard]] const std::string &content() const {
                return _content;
            }

            [[nodiscard]] std::string stringify() const {
                using namespace std::string_literals;
                return (_isSnippet ? "1"s : "0"s).append(_content);
            }

        private:
            const bool _isSnippet;
            const std::string _content;
        };

        std::optional<std::pair<char, std::optional<Completion>>> previous();

        std::optional<std::pair<char, std::optional<Completion>>> next();

        Completion reset(bool isSnippet = false, std::string content = {});

        bool valid() const;

    private:
        mutable std::shared_mutex _shared_mutex;

        std::atomic<bool> _isSnippet = false;
        std::string _content;
        std::atomic<int64_t> _currentIndex = -1;
    };
}
