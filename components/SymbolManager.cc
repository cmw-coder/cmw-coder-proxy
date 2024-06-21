#include <filesystem>
#include <fstream>
#include <optional>
#include <ranges>
#include <regex>

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

    const auto includePattern = regex(R"~(#\s*include\s*(<([^>\s]+)>|"([^"\s]+)"))~");
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

SymbolManager::SymbolManager() = default;

SymbolManager::~SymbolManager() = default;

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

void SymbolManager::tryUpdateFile(const filesystem::path& filePath, uint32_t line) {
    thread([this, filePath, line] {
        if (const auto extension = filePath.extension();
            extension == ".c" && line < 200) {
            bool needUpdate; {
                shared_lock lock{_fileSetMutex};
                needUpdate = !_fileSet.contains(filePath);
            }
            if (needUpdate) {
                _collectIncludes(filePath);
            }
            ignore = _updateTags();
        } else if (extension == ".h") {
            unique_lock lock{_tagFileMutex};
            // TODO: Check if need InteractionMonitor::GetInstance()->getInteractionLock();
            if (const auto tagsFilePath = MemoryManipulator::GetInstance()->getProjectDirectory() / "tags";
                exists(tagsFilePath)) {
                remove(MemoryManipulator::GetInstance()->getProjectDirectory() / "tags");
            }
        }
    }).detach();
}

void SymbolManager::_collectIncludes(const filesystem::path& filePath) {
    optional<filesystem::path> publicPathOpt; {
        auto absoluteFilePath = absolute(filePath);
        while (absoluteFilePath != absoluteFilePath.parent_path()) {
            if (const auto tempPath = absoluteFilePath / "PUBLIC" / "include" / "comware";
                exists(tempPath)) {
                publicPathOpt.emplace(tempPath.lexically_normal());
                break;
            }
            absoluteFilePath = absoluteFilePath.parent_path();
        }
    }
    unordered_set<filesystem::path> result = _getIncludesInFile(filePath, publicPathOpt)/*, newIncludeSet = result*/;
    // while (!newIncludeSet.empty()) {
    //     unordered_set<filesystem::path> tempIncludeSet;
    //     for (const auto& includePath: newIncludeSet) {
    //         const auto tempIncludes = _getIncludesInFile(includePath, publicPathOpt);
    //         tempIncludeSet.insert(tempIncludes.begin(), tempIncludes.end());
    //     }
    //     newIncludeSet.clear();
    //     for (const auto& tempIncludePath: tempIncludeSet) {
    //         if (result.insert(tempIncludePath).second) {
    //             newIncludeSet.insert(tempIncludePath);
    //         }
    //     }
    // }
    result.emplace(filePath); {
        unique_lock lock{_fileSetMutex};
        _fileSet.merge(result);
    }
    // logger::warn(format("Found duplicated include: {}", nlohmann::json(result).dump()));
}

unordered_set<filesystem::path> SymbolManager::_getIncludesInFile(
    const filesystem::path& filePath,
    const optional<filesystem::path>& publicPathOpt
) const {
    const auto relativePath = is_directory(filePath) ? filePath : filePath.parent_path();
    unordered_set<filesystem::path> result;
    vector<string> totalLines;
    totalLines.reserve(200); {
        ifstream fileStream{filePath};
        string line;
        for (auto index = 0; index < 200 && getline(fileStream, line); index++) {
            totalLines.push_back(line);
        }
    }
    for (const auto& line: totalLines) {
        if (smatch match; regex_search(line, match, includePattern)) {
            if (const auto pathToCheck = match[2].matched
                                             ? publicPathOpt.value_or(relativePath) / match[2].str()
                                             : relativePath / match[3].str();
                exists(pathToCheck)) {
                result.insert(pathToCheck.lexically_normal());
            }
        }
    }
    return result;
}

bool SymbolManager::_updateTags() const {
    string fileList; {
        shared_lock lock{_fileSetMutex};
        if (_fileSet.empty()) {
            return false;
        }
        for (const auto& file: _fileSet) {
            fileList += format(R"("{}" )", file.generic_string());
        }
    }
    const string arguments = format(
        R"(-a --excmd=combine -f "{}" --fields=+e+n --kinds-c=-efhmv {})",
        (MemoryManipulator::GetInstance()->getProjectDirectory() / "tags").generic_string(),
        fileList
    );
    unique_lock lock{_tagFileMutex};
    return system::runCommand("ctags.exe", arguments);
}
