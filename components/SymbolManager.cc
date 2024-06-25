#include <filesystem>
#include <optional>
#include <ranges>
#include <regex>
#include <unordered_set>

#include <magic_enum.hpp>
#include <nlohmann/json.hpp>
#include <readtags.h>

#include <components/MemoryManipulator.h>
#include <components/SymbolManager.h>
#include <utils/iconv.h>
#include <utils/logger.h>
#include <utils/system.h>

using namespace components;
using namespace magic_enum;
using namespace models;
using namespace std;
using namespace utils;

namespace {
    struct SymbolCollection {
        unordered_set<string> references, unknown;
    };

    const auto symbolPattern = regex(R"~(\b[A-Z][A-Z0-9]*(_[A-Z0-9]+)*\b)~");

    SymbolCollection collectSymbols(const string& prefixLines) {
        SymbolCollection result;
        vector<string> symbolList;
        copy(
            sregex_token_iterator(prefixLines.begin(), prefixLines.end(), symbolPattern),
            sregex_token_iterator(),
            back_inserter(symbolList)
        );
        for (const auto& symbol: symbolList) {
            if (symbol.length() < 2) {
                continue;
            }
            if (const auto lastTwoChars = symbol.substr(symbol.length() - 2);
                lastTwoChars == "_E" || lastTwoChars == "_S") {
                result.references.emplace(symbol);
            } else {
                result.unknown.emplace(symbol);
            }
        }
        return result;
    }
}

SymbolManager::SymbolManager() {
    _threadUpdateTags();
}

SymbolManager::~SymbolManager() {
    _isRunning.store(false);
}

vector<SymbolInfo> SymbolManager::getSymbols(const string& prefix) {
    vector<SymbolInfo> result;
    const auto tagsFilePath = MemoryManipulator::GetInstance()->getProjectDirectory() / "tags";
    if (!exists(tagsFilePath)) {
        return result;
    }
    const auto getEndLine = [](const vector<pair<string, string>>& symbolFields)-> optional<uint32_t> {
        if (symbolFields.size() == 1) {
            if (const auto& [key, value] = symbolFields[0];
                key == "end") {
                return stoul(value);
            }
        }
        return nullopt;
    };
    const auto getSymbolFields = [](const decltype(tagEntry::fields)& fields) -> vector<pair<string, string>> {
        vector<pair<string, string>> symbolFields;
        for (auto index = 0; index < fields.count; ++index) {
            symbolFields.emplace_back((fields.list + index)->key, (fields.list + index)->value);
        }
        return symbolFields;
    };
    const auto getTypeReference = [](
        const vector<pair<string, string>>& symbolFields
    ) -> optional<pair<string, string>> {
        if (symbolFields.size() == 1) {
            const auto& [key, value] = symbolFields[0];
            if (const auto offset = value.find(':');
                key == "typeref" && offset != string::npos) {
                return make_pair(value.substr(0, offset), value.substr(offset + 1));
            }
        }
        return nullopt;
    };
    tagFileInfo taginfo;

    shared_lock lock{_tagFileMutex};
    const auto tagsFileHandle = tagsOpen(tagsFilePath.generic_string().c_str(), &taginfo);
    if (tagsFileHandle == nullptr) {
        logger::warn("Failed to open tags file");
        return result;
    }
    for (const auto& symbol: collectSymbols(prefix).references) {
        try {
            tagEntry entry;
            if (tagsFind(tagsFileHandle, &entry, symbol.c_str(), TAG_OBSERVECASE) == TagSuccess) {
                if (const auto typeReferenceOpt = getTypeReference(getSymbolFields(entry.fields));
                    typeReferenceOpt.has_value()) {
                    if (const auto& [type, reference] = typeReferenceOpt.value();
                        tagsFind(tagsFileHandle, &entry, reference.c_str(), TAG_OBSERVECASE) == TagSuccess) {
                        if (const auto endLineOpt = getEndLine(getSymbolFields(entry.fields));
                            endLineOpt.has_value()) {
                            result.emplace_back(
                                iconv::toPath(entry.file),
                                entry.name,
                                type,
                                static_cast<uint32_t>(entry.address.lineNumber - 1),
                                endLineOpt.value() - 1
                            );
                        }
                    }
                }
            }
        } catch (exception& e) {
            logger::warn(format("Exception when getting info of symbol '{}': {}", symbol, e.what()));
        }
    }
    tagsClose(tagsFileHandle);
    return result;
}

void SymbolManager::updateRootPath(const std::filesystem::path& currentFilePath) {
    thread([this, currentFilePath] {
        auto tempPath = absolute(currentFilePath).lexically_normal();
        while (tempPath != tempPath.parent_path()) {
            if (exists(tempPath / "PUBLIC" / "include" / "comware")) {
                bool isSameRoot; {
                    shared_lock lock{_rootPathMutex};
                    isSameRoot = tempPath == _rootPath;
                }
                if (!isSameRoot) {
                    unique_lock lock{_rootPathMutex};
                    _rootPath = tempPath;
                }
                break;
            }
            tempPath = tempPath.parent_path();
        }
        _needUpdateTags.store(true);
    }).detach();
}

void SymbolManager::_threadUpdateTags() const {
    thread([this] {
        while (_isRunning) {
            if (_needUpdateTags.load()) {
                string arguments; {
                    shared_lock lock{_rootPathMutex};
                    arguments = format(
                        R"(-a --excmd=combine -f "{}" --fields=+e+n --kinds-c=-ehmv --languages=C,C++ -R {})",
                        (MemoryManipulator::GetInstance()->getProjectDirectory() / _tempTagFile).generic_string(),
                        _rootPath.generic_string()
                    );
                }
                try {
                    system::runCommand("ctags.exe", arguments);
                    unique_lock lock{_tagFileMutex};
                    rename(
                        MemoryManipulator::GetInstance()->getProjectDirectory() / _tempTagFile,
                        MemoryManipulator::GetInstance()->getProjectDirectory() / _tagFile
                    );
                } catch (exception& e) {
                    logger::warn(format("Exception when updating tags: {}", e.what()));
                }
            }
            this_thread::sleep_for(1s);
        }
    }).detach();
}
