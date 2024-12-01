#include <types/Completions.h>

#include <utility>

using namespace std;
using namespace types;

Completions::Completions(
    string actionId,
    const CompletionComponents::GenerateType generateType,
    const vector<pair<string, Selection>>& candidates
): actionId(move(actionId)), generateType(generateType), _candidates(candidates) {}

tuple<string, Selection, uint32_t> Completions::current() const {
    const auto& [content, selection] = _candidates[_currentIndex];
    return {content, selection, _currentIndex};
}

bool Completions::empty() const {
    return _candidates.empty();
}

tuple<string, Selection, uint32_t> Completions::next() {
    if (_currentIndex < _candidates.size() - 1) {
        _currentIndex++;
    }
    return current();
}

tuple<string, Selection, uint32_t> Completions::previous() {
    if (_currentIndex > 0) {
        _currentIndex--;
    }
    return current();
}
