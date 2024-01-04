#include <types/CompactString.h>

using namespace std;
using namespace types;

string CompactString::str() const {
    return {_data.content, _data.lengthLow + static_cast<uint32_t>(_data.lengthHigh << 8)};
}

CompactString::Data* CompactString::data() {
    return &_data;
}
