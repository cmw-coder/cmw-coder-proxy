#include <regex>

#include <models/MemoryPayloads.h>

using namespace models;
using namespace std;

namespace {
    regex recordRegex(
        R"~(Symbol="(.*?)";Type="(.*?)";Project="(.*?)";File="(.*?)";)~"s +
        R"~(lnFirst="(.*?)";lnLim="(.*?)";lnName="(.*?)";ichName="(.*?)";Instance="(.*?)")~"s
    );
}

SimpleString::SimpleString(std::string_view str) {
    _data.lengthLow = static_cast<uint8_t>(str.length() & 0xFF);
    _data.lengthHigh = static_cast<uint8_t>(str.length() >> 8 & 0xFF);
    ranges::copy(str, _data.content);
}

uint32_t SimpleString::length() const {
    return _data.lengthLow + static_cast<uint32_t>(_data.lengthHigh << 8);
}

string SimpleString::str() const {
    return {_data.content, length()};
}

uint32_t SymbolList::count() const {
    return _data.count;
}

uint32_t SymbolName::depth() const {
    return _data.nestingDepth;
}

string SymbolName::name() const {
    return {_data.name};
}

optional<SymbolRecord::Record> SymbolRecord::parse() const {
    const auto content = this->str();
    if (smatch match;
        regex_search(content, match, recordRegex) && match.size() == 10) {
        return Record{
            match[4],
            match[3],
            match[1],
            match[2],
            {
                static_cast<uint32_t>(stoul(match[8])),
                static_cast<uint32_t>(stoul(match[7]))
            },
            static_cast<uint32_t>(stoul(match[9])),
            static_cast<uint32_t>(stoul(match[6])),
            static_cast<uint32_t>(stoul(match[5])),
        };
    }
    return nullopt;
}
