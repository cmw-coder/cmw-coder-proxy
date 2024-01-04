#include <types/CompactString.h>

using namespace std;
using namespace types;

CompactString::CompactString(const std::string_view str) {
    _data.lengthLow = static_cast<uint8_t>(str.length() & 0xFF);
    _data.lengthHigh = static_cast<uint8_t>(str.length() >> 8 & 0xFF);
    memcpy(_data.content, str.data(), str.length());
}

string CompactString::str() const {
    return {_data.content, _data.lengthLow + static_cast<uint32_t>(_data.lengthHigh << 8)};
}

CompactString::Data* CompactString::data() {
    return &_data;
}
