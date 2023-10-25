#include <types/CompletionCache.h>

using namespace std;
using namespace types;

bool CompletionCache::empty() const {
    shared_lock<shared_mutex> lock(_shared_mutex);
    return _content.empty();
}

optional<string> CompletionCache::get() const {
    shared_lock<shared_mutex> lock(_shared_mutex);
    if (_currentIndex >= _content.length()) {
        return nullopt;
    }
    return _content[0] + _content.substr(_currentIndex);
}

optional<string> CompletionCache::getPrevious() {
    --_currentIndex;
    if (_currentIndex < 1) {
        reset();
        return nullopt;
    }
    return get();
}

std::optional<std::string> CompletionCache::getNext() {
    ++_currentIndex;
    if (_currentIndex >= _content.length()) {
        reset();
        return nullopt;
    }
    return get();
}

string CompletionCache::reset(string content) {
    unique_lock<shared_mutex> lock(_shared_mutex);
    const auto old_content = ::move(_content);
    _content = ::move(content);
    _currentIndex = 1;
    return old_content;
}

bool CompletionCache::test(char c) const {
    shared_lock<shared_mutex> lock(_shared_mutex);
    if (_currentIndex >= _content.length()) {
        return false;
    }
    return _content[_currentIndex] == c;
}
