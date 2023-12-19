//
// Created by ckf9674 on 2023/12/15.
//
#include <format>

#include <types/Range.h>
#include <utils/logger.h>

using namespace types;
using namespace utils;

Range::Range(CaretPosition start, CaretPosition end): start(start), end(end) {
    if (start > end) {
        this->start = end;
        this->end = start;
    }
}

Range::Range(uint32_t startLine, uint32_t startCharacter, uint32_t endLine, uint32_t endCharacter) {
    if (startLine < endLine || (startLine == endLine && startCharacter <= endCharacter) ) {
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

Range Range::with(const std::any& start, const std::any& end) const {
    CaretPosition tmpStart = this->start;
    CaretPosition tmpEnd = this->end;
    try {
        if (start.has_value()) {
            tmpStart = std::any_cast<CaretPosition>(start);
        }
        if (end.has_value()) {
            tmpEnd = std::any_cast<CaretPosition>(start);
        }
    } catch (const std::bad_any_cast& e) {
        logger::error(std::format("Invalid CaretPosition: {}", e.what()));
    } catch (std::out_of_range&) {
    }
    return {tmpStart, tmpEnd};
}

Range Range::Union(const Range& other) const {
    return {
        std::min(start, other.start),
        std::max(end, other.end)
    };
}

Range Range::intersection(const Range& other) const {
    return {
        std::max(start, other.start),
        std::min(end, other.end)
    };
}

std::ostream& types::operator<<(std::ostream& os, const Range& range) {
    os << range.start << " " << range.end;
    return os;
}
