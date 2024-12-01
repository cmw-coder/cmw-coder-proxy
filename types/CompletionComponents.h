#pragma once

#include <filesystem>

#include <models/SymbolInfo.h>
#include <nlohmann/json.hpp>

#include <types/CaretPosition.h>

namespace types {
    class CompletionComponents {
    public:
        enum class GenerateType {
            Common,
            Paste,
        };

        CompletionComponents(
            GenerateType generateType,
            const CaretPosition& caretPosition,
            const std::filesystem::path& path
        );

        void setContext(
            const std::string& prefix,
            const std::string& infix,
            const std::string& suffix
        );

        void setRecentFiles(const std::vector<std::filesystem::path>& recentFiles);

        void setSymbols(const std::vector<models::SymbolInfo>& symbols);

        nlohmann::json toJson() const;

    private:
        GenerateType _generateType;
        CaretPosition _caretPosition;
        std::filesystem::path _path;
        std::string _infix, _prefix, _suffix;
        std::vector<std::filesystem::path> _recentFiles;
        std::vector<models::SymbolInfo> _symbols;
        std::chrono::time_point<std::chrono::system_clock> _initTime, _contextTime, _recentFilesTime, _symbolTime;
    };
}
