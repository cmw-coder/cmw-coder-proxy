#include <format>

#include <types/Range.h>
#include <utils/logger.h>

using namespace std;
using namespace types;
using namespace utils;

Range::Range(const CaretPosition& start, const CaretPosition& end): start(start), end(end) {
    if (start > end) {
        this->start = end;
        this->end = start;
    }
}

Range::Range(
    const uint32_t startLine,
    const uint32_t startCharacter,
    const uint32_t endLine,
    const uint32_t endCharacter
) {
    if (startLine < endLine || (startLine == endLine && startCharacter <= endCharacter)) {
        this->start = CaretPosition(startCharacter, startLine);
        this->end = CaretPosition(endCharacter, endLine);
    } else {
        this->end = CaretPosition(startCharacter, startLine);
        this->start = CaretPosition(endCharacter, endLine);
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

Range Range::Union(const Range& other) const {
    return {
        min(start, other.start),
        max(end, other.end)
    };
}

Range Range::intersection(const Range& other) const {
    return {
        max(start, other.start),
        min(end, other.end)
    };
}
