#pragma once

#include <optional>
#include <shared_mutex>
#include <string>

namespace types {
    class CompletionCache {
    public:
        bool empty() const;

        std::optional<std::string> get() const;

        std::optional<std::string> getPrevious();

        std::optional<std::string> getNext();

        std::string reset(std::string &&content = {});

        bool test(char c) const;

    private:
        mutable std::shared_mutex _shared_mutex;

        std::string _content;
        std::atomic<int64_t> _currentIndex = 1;
    };
}
