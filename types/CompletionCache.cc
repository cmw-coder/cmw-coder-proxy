#include <types/CompletionCache.h>

using namespace std;
using namespace types;

optional<pair<char, optional<string>>> CompletionCache::previous() {
    if (!valid()) {
        return nullopt;
    }

    const auto currentChar = _content[_index];

    if (_index > 0) {
        --_index;
        return make_pair(currentChar, _content.substr(_index));
    }

    return make_pair(currentChar, nullopt);
}

optional<pair<char, optional<string>>> CompletionCache::next() {
    if (!valid()) {
        return nullopt;
    }

    const auto currentChar = _content[_index];

    if (_index < _content.length() - 1) {
        ++_index;
        return make_pair(currentChar, _content.substr(_index));
    }

    return make_pair(currentChar, nullopt);
}

pair<string, int64_t> CompletionCache::reset(string content) {
    const auto previousState = make_pair(_content, _index);
    _content = move(content);
    _index = _content.empty() ? -1 : 0;
    return previousState;
}

bool CompletionCache::valid() const {
    return _index >= 0;
}
