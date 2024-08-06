#pragma once

#include <singleton_dclp.hpp>

#include <models/ReviewReference.h>
#include <models/SymbolInfo.h>
#include <types/ConstMap.h>

namespace components {
    class SymbolManager : public SingletonDclp<SymbolManager> {
    public:
        enum class TagFileType {
            Function,
            Structure,
        };

        SymbolManager();

        ~SymbolManager() override;

        std::unordered_map<std::string, models::ReviewReference> getReviewReferences(
            const std::string& content,
            const std::filesystem::path& referencePath,
            uint32_t targetDepth = 0
        ) const;

        std::vector<models::SymbolInfo> getSymbols(
            const std::string& content,
            const std::filesystem::path& referencePath,
            bool full = false
        ) const;

        void updateRootPath(const std::filesystem::path& currentFilePath);

    private:
        const types::EnumMap<TagFileType, const char *> _tagKindsMap = {
            {
                {
                    {TagFileType::Function, "df"},
                    {TagFileType::Structure, "egstuv"}
                }
            }
        };
        const types::EnumMap<TagFileType, std::pair<const char *, const char *>> _tagFilenameMap = {
            {
                {
                    {TagFileType::Function, {"function.ctags", "function.tmp"}},
                    {TagFileType::Structure, {"structure.ctags", "structure.tmp"}}
                }
            }
        };
        std::unordered_map<TagFileType, bool> _tagFileNeedUpdateMap = {
            {TagFileType::Function, false},
            {TagFileType::Function, false}
        };
        mutable std::shared_mutex _rootPathMutex, _functionTagFileMutex, _structureTagFileMutex;
        std::atomic<bool> _isRunning{true}, _functionTagFileNeedUpdate{false}, _structureTagFileNeedUpdate{false};
        std::filesystem::path _rootPath;

        std::unordered_map<std::string, models::ReviewReference> _getReferences(
            const std::string& content,
            const std::filesystem::path& referencePath,
            uint32_t depth
        ) const;

        void _threadUpdateFunctionTagFile();

        void _threadUpdateStructureTagFile();

        void _updateTagFile(TagFileType tagFileType);
    };
}
