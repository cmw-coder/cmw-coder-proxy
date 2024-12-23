#pragma once

#include <filesystem>

#include <nlohmann/json.hpp>

#include <models/SymbolInfo.h>
#include <types/CaretPosition.h>

namespace types {
    class CompletionComponents {
    public:
        enum class GenerateType {
            Common,
            PasteReplace,
        };

        const std::filesystem::path path;

        CompletionComponents(
            GenerateType generateType,
            const CaretPosition& caretPosition,
            const std::filesystem::path& path
        );

        std::string getPrefix() const;

        std::vector<std::filesystem::path> getRecentFiles() const;

        std::string getSuffix() const;

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
        std::string _infix, _prefix, _suffix;
        std::vector<std::filesystem::path> _recentFiles;
        std::vector<models::SymbolInfo> _symbols;
        std::chrono::time_point<std::chrono::system_clock> _initTime, _contextTime, _recentFilesTime, _symbolTime;
    };
}
