#pragma once

#include <filesystem>

namespace models {
    struct SymbolInfo {
        enum class Type {
            Enum,
            Function,
            Macro,
            Reference,
            Struct,
            Unknown,
            Variable,
        };

        std::filesystem::path path;
        std::string name;
        Type type;
        uint32_t startLine, endLine;
    };
}
