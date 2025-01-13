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

        [[nodiscard]] std::string getPrefix() const;

        [[nodiscard]] std::vector<std::filesystem::path> getRecentFiles() const;

        [[nodiscard]] std::string getSuffix() const;

        [[nodiscard]] bool needCache(const CaretPosition& caretPosition) const;

        void setContext(
            const std::string& prefix,
            const std::string& infix,
            const std::string& suffix
        );

        void setRecentFiles(const std::vector<std::filesystem::path>& recentFiles);

        void setSymbols(const std::vector<models::SymbolInfo>& symbols);

        [[nodiscard]] nlohmann::json toJson() const;

        void updateCaretPosition(const CaretPosition& caretPosition);

        void useCachedContext(
            const std::string& currentLinePrefix,
            const std::string& newInfix,
            const std::string& currentLineSuffix
        );

    private:
        GenerateType _generateType;
        CaretPosition _caretPosition;
        std::string _infix, _prefix, _suffix;
        std::vector<std::filesystem::path> _recentFiles;
        std::vector<models::SymbolInfo> _symbols;
        std::chrono::time_point<std::chrono::system_clock> _initTime, _contextTime, _recentFilesTime, _symbolTime;

        void _resetTimePoints();
    };
}
