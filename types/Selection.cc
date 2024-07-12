#include <format>

#include <types/Selection.h>
#include <utils/logger.h>

using namespace std;
using namespace types;
using namespace utils;

Selection::Selection(const CaretPosition& begin, const CaretPosition& end): begin(begin), end(end) {
    if (begin > end) {
        this->begin = end;
        this->end = begin;
    }
}

bool Selection::hasIntersection(const Selection& other) const {
    return begin <= other.end && other.begin <= end;
}

bool Selection::isEmpty() const {
    return begin == end;
}

bool Selection::isSingleLine() const {
    return begin.line == end.line && begin.character != end.character;
}

bool Selection::isEqual(const Selection& other) const {
    return begin == other.begin && end == other.end;
}

void Selection::reset() {
    end.reset();
    begin.reset();
}
