#pragma once

#include <optional>
#include <shared_mutex>

#include <types/Completion.h>

namespace types {
    class CompletionCache {
    public:
        std::optional<std::pair<char, std::optional<Completion>>> previous();

        std::optional<std::pair<char, std::optional<Completion>>> next();
        
        Completion completion();

        Completion reset(bool isSnippet = false, std::string content = {});

        bool valid() const;

    private:
        mutable std::shared_mutex _shared_mutex;

        std::atomic<bool> _isSnippet = false;
        std::string _content;
        std::atomic<int64_t> _currentIndex = -1;
    };
}
