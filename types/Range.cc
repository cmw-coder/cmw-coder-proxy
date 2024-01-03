#include <format>

#include <types/Range.h>
#include <utils/logger.h>

using namespace std;
using namespace types;
using namespace utils;

Range::Range(const CaretPosition& start, const CaretPosition& end): end(end), start(start) {
    if (start > end) {
        this->start = end;
        this->end = start;
    }
}

bool Range::isEmpty() const {
    return start == end;
}

bool Range::isSingleLine() const {
    return start.line == end.line && start.character != end.character;
}

bool Range::contains(const Range& other) const {
    return start >= other.start && other.end <= end;
}

bool Range::isEqual(const Range& other) const {
    return start == other.start && end == other.end;
}

bool Range::isBefore(const Range& other) const {
    return end > other.start;
}

void Range::reset() {
    end.reset();
    start.reset();
}
