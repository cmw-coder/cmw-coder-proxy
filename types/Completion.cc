#include <types/Completion.h>

using namespace std;
using namespace types;

Completion::Completion(bool isSnippet, string content) :
        _isSnippet(isSnippet),
        _content(std::move(content)) {}

bool Completion::isSnippet() const {
    return _isSnippet;
}

const string &Completion::content() const {
    return _content;
}

string Completion::stringify() const {
    using namespace string_literals;
    return (_isSnippet ? "1"s : "0"s).append(_content);
}
