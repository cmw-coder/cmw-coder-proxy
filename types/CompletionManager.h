#pragma once

#include <types/CompletionCache.h>

namespace types {
    class CompletionManager {
    public:
        void acceptCompletion();
    private:
        CompletionCache _completionCache;
        std::atomic<bool> _isAutoCompletion = true, _isJustInserted = false;
    };
}
