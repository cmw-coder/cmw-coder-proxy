#pragma once

#include <models/SymbolInfo.h>

namespace models {
    struct ReviewReference {
        std::filesystem::path path;
        std::string name, content;
        SymbolInfo::Type type;
        uint32_t startLine, endLine, depth;
    };
}
