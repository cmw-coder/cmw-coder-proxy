#include <types/CompletionCache.h>

using namespace std;
using namespace types;

optional<pair<char, optional<Completion>>> CompletionCache::previous() {
    if (!valid()) {
        return nullopt;
    }

    shared_lock lock(_shared_mutex);
    const auto currentChar = _content[_currentIndex];
    if (_currentIndex > 0) {
        --_currentIndex;
        return make_pair(currentChar, Completion{_isSnippet.load(), _content.substr(_currentIndex)});
    }
    return make_pair(currentChar, nullopt);
}

optional<pair<char, optional<Completion>>> CompletionCache::next() {
    if (!valid()) {
        return nullopt;
    }

    shared_lock lock(_shared_mutex);
    const auto currentChar = _content[_currentIndex];
    if (_currentIndex < _content.length() - 1) {
        ++_currentIndex;
        return make_pair(currentChar, Completion{_isSnippet.load(), _content.substr(_currentIndex)});
    }
    return make_pair(currentChar, nullopt);
}

Completion CompletionCache::current() {
    shared_lock lock(_shared_mutex);
    return {_isSnippet, _content};
}

Completion CompletionCache::reset(const bool isSnippet, string content) {
    _currentIndex = content.empty() ? -1 : 0;

    unique_lock lock(_shared_mutex);
    const auto oldCompletion = Completion{_isSnippet, _content};
    _isSnippet = isSnippet;
    _content = move(content);
    return oldCompletion;
}

bool CompletionCache::valid() const {
    return _currentIndex >= 0;
}
