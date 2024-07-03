#include <format>

#include <types/Selection.h>
#include <utils/logger.h>

using namespace std;
using namespace types;
using namespace utils;

Selection::Selection(const CaretPosition& start, const CaretPosition& end): end(end), start(start) {
    if (start > end) {
        this->start = end;
        this->end = start;
    }
}

bool Selection::isEmpty() const {
    return start == end;
}

bool Selection::isSingleLine() const {
    return start.line == end.line && start.character != end.character;
}

bool Selection::contains(const Selection& other) const {
    return start >= other.start && other.end <= end;
}

bool Selection::isEqual(const Selection& other) const {
    return start == other.start && end == other.end;
}

bool Selection::isBefore(const Selection& other) const {
    return end > other.start;
}

void Selection::reset() {
    end.reset();
    start.reset();
}
