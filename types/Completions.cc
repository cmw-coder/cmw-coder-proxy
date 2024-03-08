#include <types/Completions.h>

#include <utility>

using namespace std;
using namespace types;

Completions::Completions(string actionId, const vector<string>& candidates)
    : actionId(move(actionId)), _candidates(candidates) {}

uint32_t Completions::size() const {
    return _candidates.size();
}

tuple<string, uint32_t> Completions::current() const {
    return {_candidates[_currentIndex], _currentIndex};
}

tuple<string, uint32_t> Completions::next() {
    if (_currentIndex < _candidates.size() - 1) {
        _currentIndex++;
    }
    return current();
}

tuple<string, uint32_t> Completions::previous() {
    if (_currentIndex > 0) {
        _currentIndex--;
    }
    return current();
}
