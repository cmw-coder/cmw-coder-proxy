#pragma once

#include <singleton_dclp.hpp>

#include <models/SymbolInfo.h>

namespace components {
    class SymbolManager : public SingletonDclp<SymbolManager> {
    public:
        SymbolManager();

        ~SymbolManager() override;

        std::vector<models::SymbolInfo> getSymbols(const std::string& prefix);

        void updateRootPath(const std::filesystem::path& currentFilePath);

    private:
        const std::string _tagFile = "current.ctags", _tempTagFile = "temp.ctags";
        mutable std::shared_mutex _rootPathMutex, _tagFileMutex;
        std::atomic<bool> _isRunning{true}, _needUpdateTags{false};
        std::filesystem::path _rootPath;

        void _threadUpdateTags() const;
    };
}
