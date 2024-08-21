#include <types/CaretPosition.h>

using namespace types;

CaretPosition::CaretPosition(const uint32_t character, const uint32_t line)
    : character(character), line(line), maxCharacter(line) {}

void CaretPosition::reset() {
    this->character = 0;
    this->line = 0;
    this->maxCharacter = 0;
}

bool CaretPosition::operator<(const CaretPosition& other) const {
    return this->line < other.line || (this->line == other.line && this->character < other.character);
}

bool CaretPosition::operator<=(const CaretPosition& other) const {
    return this->line < other.line || (this->line == other.line && this->character <= other.character);
}

bool CaretPosition::operator==(const CaretPosition& other) const {
    return this->line == other.line && this->character == other.character;
}

bool CaretPosition::operator!=(const CaretPosition& other) const {
    return this->line != other.line || this->character != other.character;
}

bool CaretPosition::operator>(const CaretPosition& other) const {
    return this->line > other.line || (this->line == other.line && this->character > other.character);
}

bool CaretPosition::operator>=(const CaretPosition& other) const {
    return this->line > other.line || (this->line == other.line && this->character >= other.character);
}

CaretPosition types::operator+(const CaretPosition& lhs, const CaretPosition& rhs) {
    return {lhs.character + rhs.character, lhs.line + rhs.line};
}

CaretPosition types::operator-(const CaretPosition& lhs, const CaretPosition& rhs) {
    return {
        lhs.character < rhs.character ? 0 : lhs.character - rhs.character,
        lhs.line < rhs.line ? 0 : lhs.line - rhs.line
    };
}
