#include <models/MemoryPayloads.h>

using namespace std;
using namespace models;

SimpleString::SimpleString(const std::string_view str) {
    _data.lengthLow = static_cast<uint8_t>(str.length() & 0xFF);
    _data.lengthHigh = static_cast<uint8_t>(str.length() >> 8 & 0xFF);
    memcpy(_data.content, str.data(), str.length());
}

string SimpleString::str() const {
    return {_data.content, _data.lengthLow + static_cast<uint32_t>(_data.lengthHigh << 8)};
}

SimpleString::Data* SimpleString::data() {
    return &_data;
}

SymbolLocation::Data* SymbolLocation::data() {
    return &_data;
}
