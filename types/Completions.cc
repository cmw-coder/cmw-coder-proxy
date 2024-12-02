#include <types/Completions.h>

#include <utility>

using namespace std;
using namespace types;

Completions::Completions(
    string actionId,
    const CompletionComponents::GenerateType generateType,
    const Selection& selection,
    const vector<string>& candidates
): actionId(move(actionId)), generateType(generateType), selection(selection), _candidates(candidates) {}

tuple<string, uint32_t> Completions::current() const {
    return {_candidates[_currentIndex], _currentIndex};
}

bool Completions::empty() const {
    return _candidates.empty();
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
