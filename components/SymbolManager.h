#pragma once

#include <unordered_set>

#include <singleton_dclp.hpp>

#include <models/SymbolInfo.h>

namespace components {
    class SymbolManager : public SingletonDclp<SymbolManager> {
    public:
        SymbolManager();

        ~SymbolManager() override;

        std::vector<models::SymbolInfo> getSymbols(const std::string& prefix);

        void updateFile(const std::string& filePath);

    private:
        mutable std::shared_mutex _fileSetMutex, _tagFileMutex;
        std::unordered_set<std::filesystem::path> _fileSet;

        std::unordered_set<std::filesystem::path> _collectIncludes(const std::string& filePath) const;

        std::unordered_set<std::filesystem::path> _getIncludesInFile(
            const std::filesystem::path& filePath,
            const std::optional<std::filesystem::path>& publicPathOpt
        ) const;

        bool _updateTags(const std::filesystem::path& filePath) const;

        bool _updateTags(const std::unordered_set<std::filesystem::path>& fileSet) const;
    };
}
