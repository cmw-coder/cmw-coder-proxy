#pragma once

namespace models {
    struct SymbolInfo {
        enum class Type {
            Enum,
            Function,
            Macro,
            Reference,
            Structure,
            Unknown,
            Variable,
        };

        std::filesystem::path path;
        std::string name, type;
        uint32_t startLine, endLine;
    };
}
