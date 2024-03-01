#pragma once

namespace models {
    struct SymbolInfo {
        enum class Type {
            Constant,
            Function,
            Structure,
            Unknown,
            Variable,
        };

        std::string name, path, type;
        uint32_t startLine, endLine;
    };
}
