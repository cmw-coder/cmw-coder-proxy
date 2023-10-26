#include <types/CompletionCache.h>

using namespace std;
using namespace types;

optional<pair<char, optional<CompletionCache::Completion>>> CompletionCache::previous() {
    if (!valid()) {
        return nullopt;
    }

    shared_lock<shared_mutex> lock(_shared_mutex);
    const auto currentChar = _content[_currentIndex];
    if (_currentIndex > 0) {
        --_currentIndex;
        return make_pair(currentChar, Completion{_isSnippet.load(), _content.substr(_currentIndex)});
    }
    return make_pair(currentChar, nullopt);
}

optional<pair<char, optional<CompletionCache::Completion>>> CompletionCache::next() {
    if (!valid()) {
        return nullopt;
    }

    shared_lock<shared_mutex> lock(_shared_mutex);
    const auto currentChar = _content[_currentIndex];
    if (_currentIndex < _content.length() - 1) {
        ++_currentIndex;
        return make_pair(currentChar, Completion{_isSnippet.load(), _content.substr(_currentIndex)});
    }
    return make_pair(currentChar, nullopt);
}

string CompletionCache::reset(bool isSnippet, string content) {
    _isSnippet = isSnippet;
    _currentIndex = content.empty() ? -1 : 0;
    unique_lock<shared_mutex> lock(_shared_mutex);
    const auto old_content = ::move(_content);
    _content = ::move(content);
    return old_content;
}

bool CompletionCache::valid() const {
    return _currentIndex >= 0;
}
