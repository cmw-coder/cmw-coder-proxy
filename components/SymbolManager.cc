#include <filesystem>
#include <fstream>
#include <optional>
#include <ranges>
#include <regex>

#include <magic_enum.hpp>
#include <readtags.h>

#include <components/MemoryManipulator.h>
#include <components/SymbolManager.h>
#include <utils/fs.h>
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
        tagEntry entry;
        if (tagsFind(tagsFileHandle, &entry, symbol.c_str(), TAG_OBSERVECASE) == TagSuccess) {
            if (const auto typeReferenceOpt = getTypeReference(getSymbolFields(entry.fields));
                typeReferenceOpt.has_value()) {
                result.emplace_back(
                    entry.name,
                    entry.file,
                    string{enum_name(SymbolInfo::Type::Reference)},
                    static_cast<uint32_t>(entry.address.lineNumber),
                    static_cast<uint32_t>(entry.address.lineNumber)
                );
                if (const auto& [type, reference] = typeReferenceOpt.value();
                    tagsFind(tagsFileHandle, &entry, reference.c_str(), TAG_OBSERVECASE) == TagSuccess) {
                    if (const auto endLineOpt = getEndLine(getSymbolFields(entry.fields));
                        endLineOpt.has_value()) {
                        result.emplace_back(
                            entry.name,
                            entry.file,
                            type,
                            static_cast<uint32_t>(entry.address.lineNumber),
                            endLineOpt.value()
                        );
                    }
                }
            }
        }
    }
    tagsClose(tagsFileHandle);
    return result;
}

void SymbolManager::updateFile(const string& filePath) {
    thread([this,filePath] {
        if (const auto extension = fs::getExtension(filePath);
            extension == ".c" || extension == ".h") {
            bool needUpdate; {
                shared_lock lock{_fileSetMutex};
                needUpdate = !_fileSet.contains(filePath);
            }
            if (needUpdate) {
                auto fileSet = _collectIncludes(filePath);
                fileSet.emplace(filePath);
                if (_updateTags(fileSet)) {
                    {
                        unique_lock lock{_fileSetMutex};
                        _fileSet.merge(fileSet);
                    }
                    for (const auto& duplicated: fileSet) {
                        logger::warn(format("Found duplicated include: {}", duplicated.generic_string()));
                    }
                }
            } else {
                ignore = _updateTags(filePath);
            }
        }
    }).detach();
}

unordered_set<filesystem::path> SymbolManager::_collectIncludes(const string& filePath) const {
    optional<filesystem::path> publicPathOpt; {
        auto absoluteFilePath = filesystem::absolute(filePath);
        while (absoluteFilePath != absoluteFilePath.parent_path()) {
            if (const auto tempPath = absoluteFilePath / "PUBLIC" / "include" / "comware";
                exists(tempPath)) {
                publicPathOpt.emplace(tempPath.lexically_normal());
            }
            absoluteFilePath = absoluteFilePath.parent_path();
        }
    }
    unordered_set<filesystem::path> result = _getIncludesInFile(filePath, publicPathOpt), newIncludeSet = result;
    while (!newIncludeSet.empty()) {
        unordered_set<filesystem::path> tempIncludeSet;
        for (const auto& includePath: newIncludeSet) {
            const auto tempIncludes = _getIncludesInFile(includePath, publicPathOpt);
            tempIncludeSet.insert(tempIncludes.begin(), tempIncludes.end());
        }
        newIncludeSet.clear();
        for (const auto& tempIncludePath: tempIncludeSet) {
            if (result.insert(tempIncludePath).second) {
                newIncludeSet.insert(tempIncludePath);
            }
        }
    }
    return result;
}

unordered_set<filesystem::path> SymbolManager::_getIncludesInFile(
    const filesystem::path& filePath,
    const optional<filesystem::path>& publicPathOpt
) const {
    const auto relativePath = is_directory(filePath) ? filePath : filePath.parent_path();
    unordered_set<filesystem::path> result;
    ifstream fileStream{filePath};
    uint32_t searchLimit{};
    string line;
    while (getline(fileStream, line) && searchLimit < 100) {
        if (smatch match; regex_search(line, match, includePattern)) {
            const auto pathToCheck = match[2].matched
                                         ? publicPathOpt.value_or(relativePath) / match[2].str()
                                         : relativePath / match[3].str();
            bool needRecord; {
                shared_lock lock{_fileSetMutex};
                needRecord = !_fileSet.contains(pathToCheck) && exists(pathToCheck);
            }
            if (needRecord) {
                searchLimit = 0;
                result.insert(pathToCheck.lexically_normal());
            }
        }
        if (!result.empty()) {
            searchLimit++;
        }
    }
    return result;
}

bool SymbolManager::_updateTags(const filesystem::path& filePath) const {
    const auto arguments = format(
        R"(-a --excmd=combine -f "{}" --fields=+e+n --kinds-c=-efhmv "{}")",
        (MemoryManipulator::GetInstance()->getProjectDirectory() / "tags").generic_string(),
        filePath.generic_string()
    );
    unique_lock lock{_tagFileMutex};
    if (!system::runCommand("ctags.exe", arguments)) {
        logger::warn("Failed to generate tags");
        return false;
    }
    return true;
}

bool SymbolManager::_updateTags(const unordered_set<filesystem::path>& fileSet) const {
    if (fileSet.empty()) {
        return false;
    }
    string fileList;
    for (const auto& file: fileSet) {
        fileList += format(R"("{}" )", file.generic_string());
    }
    const string arguments = format(
        R"(-a --excmd=combine -f "{}" --fields=+e+n --kinds-c=-efhmv {})",
        (MemoryManipulator::GetInstance()->getProjectDirectory() / "tags").generic_string(),
        fileList
    );
    unique_lock lock{_tagFileMutex};
    if (!system::runCommand("ctags.exe", arguments)) {
        logger::warn("Failed to generate tags");
        return false;
    }
    return true;
}
