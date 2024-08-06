//
// Created by particleg on 24-8-6.
//

#include <types/TagEntry.h>

using namespace std;
using namespace types;

TagEntry::TagEntry(const tagEntry& entry)
    : fileScope(entry.fileScope),
      name(entry.name),
      file(entry.file),
      kind(entry.kind),
      address({entry.address.pattern, entry.address.lineNumber}) {
    for (auto index = 0; index < entry.fields.count; ++index) {
        fields.insert_or_assign((entry.fields.list + index)->key, (entry.fields.list + index)->value);
    }
}


optional<uint32_t> TagEntry::getEndLine() const {
    if (const auto key = "end";
        fields.contains(key)) {
        return stoul(fields.at(key));
    }
    return nullopt;
}

optional<pair<string, string>> TagEntry::getReferenceTarget() const {
    if (const auto key = "typeref";
        fields.contains(key)) {
        const auto value = fields.at(key);
        if (const auto offset = value.find(':');
            offset != string::npos) {
            return make_pair(value.substr(0, offset), value.substr(offset + 1));
        }
    }
    return nullopt;
}
