#include <filesystem>
#include <memory>
#include <optional>
#include <ranges>
#include <regex>
#include <unordered_set>

#include <magic_enum.hpp>
#include <nlohmann/json.hpp>
#include <readtags.h>

#include <components/MemoryManipulator.h>
#include <components/SymbolManager.h>
#include <utils/fs.h>
#include <utils/iconv.h>
#include <utils/logger.h>
#include <utils/system.h>

using namespace components;
using namespace magic_enum;
using namespace models;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    struct SymbolCollection {
        unordered_set<string> references, unknown;
    };

    class TagEntry {
    public:
        bool fileScope;
        string name, file, kind;

        struct {
            string pattern;
            unsigned long lineNumber;
        } address;

        unordered_map<string, string> fields;

        explicit TagEntry(const tagEntry& entry)
            : fileScope(entry.fileScope),
              name(entry.name),
              file(entry.file),
              kind(entry.kind),
              address({entry.address.pattern, entry.address.lineNumber}) {
            for (auto index = 0; index < entry.fields.count; ++index) {
                fields.insert_or_assign((entry.fields.list + index)->key, (entry.fields.list + index)->value);
            }
        }

        [[nodiscard]] optional<uint32_t> getEndLine() const {
            if (const auto key = "end";
                fields.contains(key)) {
                return stoul(fields.at(key));
            }
            return nullopt;
        }

        [[nodiscard]] optional<pair<string, string>> getReferenceTarget() const {
            if (const auto key = "typeref";
                fields.contains(key)) {
                const auto value = fields.at(key);
                if (const auto offset = value.find(':');
                    offset != string::npos) {
                    return make_pair(value.substr(0, offset), value.substr(offset + 1));
                }
            }
            return nullopt;
        }
    };

    const unordered_map<string, SymbolInfo::Type> symbolMapping =
    {
        {"d", SymbolInfo::Type::Macro},
        {"enum", SymbolInfo::Type::Enum},
        {"f", SymbolInfo::Type::Function},
        {"struct", SymbolInfo::Type::Struct},
    };

    const array<string, 12> excludePatterns =
    {
        "clt",
        "lib/third",
        "PUBLIC/include/comware/sys/appmonitor.h",
        "PUBLIC/include/comware/sys/assert.h",
        "PUBLIC/include/comware/sys/basetype.h",
        "PUBLIC/include/comware/sys/error.h",
        "PUBLIC/include/comware/sys/magic.h",
        "PUBLIC/include/comware/sys/overlayoam.h",
        "PUBLIC/init",
        "PUBLIC/product",
        "tools",
        "ut"
    };
    const auto symbolPattern = regex(R"~(\b[A-Z_a-z][0-9A-Z_a-z]+\b)~");

    const unordered_set<string> ignoredWords{
        // C keywords
        "alignas",
        "alignof",
        "auto",
        "bool",
        "break",
        "case",
        "char",
        "const",
        "constexpr",
        "continue",
        "default",
        "do",
        "double",
        "else",
        "enum",
        "extern",
        "false",
        "float",
        "for",
        "goto",
        "if",
        "inline",
        "int",
        "long",
        "nullptr",
        "register",
        "restrict",
        "return",
        "short",
        "signed",
        "sizeof",
        "static",
        "static_assert",
        "struct",
        "switch",
        "thread_local",
        "true",
        "typedef",
        "typeof",
        "typeof_unqual",
        "union",
        "unsigned",
        "void",
        "volatile",
        "while",
        "_Alignas",
        "_Alignof",
        "_Atomic",
        "_BitInt",
        "_Bool",
        "_Complex",
        "_Decimal128",
        "_Decimal32",
        "_Decimal64",
        "_Generic",
        "_Imaginary",
        "_Noreturn",
        "_Static_assert",
        "_Thread_local",
        "__innerSASSERTCORE",
        "__innerSASSERTCORE2",
        "_ALWAYS_INLINE",
        "_SYS_BASETYPE_H_",
        // Base types
        "ARRAY_SIZE",
        "BIT_COMPARE",
        "BIT_MATCH",
        "BIT_RESET",
        "BIT_SET",
        "BIT_TEST",
        "BOOL_FALSE",
        "BOOL_T",
        "BOOL_TRUE",
        "CA_BUILD_FAIL",
        "CHAR",
        "COMWARE_LEOPARD_VERSION",
        "container_of",
        "DISABLE",
        "DOUBLE",
        "ENABLE",
        "FLOAT",
        "FROZEN_IMPL",
        "hton64",
        "htonl",
        "htons",
        "IGNORE_PARAM",
        "IN",
        "INLINE",
        "INOUT",
        "INT",
        "INT16",
        "INT32",
        "INT64",
        "INT8",
        "ISSU",
        "ISSUASSERT",
        "likely",
        "LONG",
        "LPVOID",
        "MAC_ADDR_LEN",
        "MAX",
        "MIN",
        "MODULE_CONSTRUCT",
        "MODULE_DESTRUCT",
        "NOINLSTATIC",
        "ntoh64",
        "ntohl",
        "ntohs",
        "offsetof",
        "OUT",
        "SHORT",
        "STATIC",
        "STATICASSERT",
        "UCHAR",
        "UINT",
        "UINT16",
        "UINT32",
        "UINT64",
        "UINT8",
        "ULONG",
        "unlikely",
        "USHORT",
        "VOID",
    };

    const array<filesystem::path, 27> modulePaths{
        "ACCESS/src",
        "CRYPTO/src",
        "DC/src",
        "DEV/src",
        "DLP/src",
        "DPI/src",
        "DRV_SIMSWITCH/src",
        "DRV_SIMWARE9/src",
        "FE/src",
        "FW/src",
        "IP/src",
        "L2VPN/src",
        "LAN/src",
        "LB/src",
        "LINK/src",
        "LSM/src",
        "MCAST/src",
        "NETFWD/src",
        "OFP/src",
        "PSEC/src",
        "PUBLIC/include/comware",
        "QACL/src",
        "TEST/src",
        "VOICE/src",
        "VPN/src",
        "WLAN/src",
        "X86PLAT/src",
    };

    SymbolCollection collectSymbols(const string& prefixLines) {
        SymbolCollection result;
        vector<string> symbolList;
        copy(
            sregex_token_iterator(prefixLines.begin(), prefixLines.end(), symbolPattern),
            sregex_token_iterator(),
            back_inserter(symbolList)
        );
        for (const auto& symbol: symbolList) {
            if (symbol.length() < 10 || ignoredWords.contains(symbol)) {
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

    optional<TagEntry> findMostCommonPathSymbol(
        const shared_ptr<tagFile>& tagFileHandle,
        const string& symbol,
        const filesystem::path& referencePath
    ) {
        uint32_t mostCommonPathLength{};
        tagEntry entry{};
        optional<TagEntry> result;
        if (tagsFind(tagFileHandle.get(), &entry, symbol.c_str(), TAG_OBSERVECASE) == TagSuccess) {
            const auto referencePathString = referencePath.generic_string();
            if (const auto pathDistance = distance(
                referencePathString.cbegin(), ranges::mismatch(
                    referencePathString,
                    filesystem::path(entry.file).generic_string()
                ).in1
            ); pathDistance > mostCommonPathLength) {
                mostCommonPathLength = pathDistance;
                result.emplace(entry);
            }
            while (tagsFindNext(tagFileHandle.get(), &entry) == TagSuccess) {
                if (const auto pathDistance = distance(
                    referencePathString.cbegin(), ranges::mismatch(
                        referencePathString,
                        filesystem::path(entry.file).generic_string()
                    ).in1
                ); pathDistance > mostCommonPathLength) {
                    mostCommonPathLength = pathDistance;
                    result.emplace(entry);
                }
            }
        }
        return result;
    }
}

SymbolManager::SymbolManager() {
    _threadUpdateFunctionTagFile();
    _threadUpdateStructureTagFile();
}

SymbolManager::~SymbolManager() {
    _isRunning.store(false);
}

unordered_map<string, ReviewReference> SymbolManager::getReviewReferences(
    const string& content,
    const filesystem::path& referencePath,
    const uint32_t targetDepth
) const {
    uint32_t currentDepth = 0;
    auto reviewReferences = _getReferences(content, referencePath, currentDepth);

    while (currentDepth < targetDepth) {
        ++currentDepth;

        mutex reviewReferencesMutex;
        vector<thread> retriveReferenceThreads;
        retriveReferenceThreads.reserve(reviewReferences.size());
        for (auto& [key, value]: unordered_map{reviewReferences}) {
            retriveReferenceThreads.emplace_back(
                [tempContent = move(value.content), tempPath = move(value.path),
                    currentDepth, &reviewReferences, &reviewReferencesMutex, this] {
                    auto tempReferences = _getReferences(tempContent, tempPath, currentDepth);
                    unique_lock lock{reviewReferencesMutex};
                    reviewReferences.merge(move(tempReferences));
                }
            );
        }
        for (auto& retriveReferenceThread: retriveReferenceThreads) {
            retriveReferenceThread.join();
        }
    }

    return reviewReferences;
}

vector<SymbolInfo> SymbolManager::getSymbols(
    const string& content,
    const filesystem::path& referencePath,
    const bool full
) const {
    vector<SymbolInfo> result; {
        const auto tagFilePath = MemoryManipulator::GetInstance()->getProjectDirectory()
                                 / _tagFilenameMap.at(TagFileType::Structure).first;
        if (!exists(tagFilePath)) {
            return result;
        }

        shared_lock lock{_structureTagFileMutex};
        const shared_ptr<tagFile> tagFileHandle(tagsOpen(tagFilePath.generic_string().c_str(), nullptr), tagsClose);
        if (tagFileHandle == nullptr) {
            logger::warn(format("Failed to open '{}'", tagFilePath.generic_string()));
            return result;
        }

        for (const auto& typeReferenceString: collectSymbols(content).references) {
            try {
                if (const auto referenceEntryOpt = findMostCommonPathSymbol(
                    tagFileHandle,
                    typeReferenceString,
                    referencePath
                ); referenceEntryOpt.has_value()) {
                    if (const auto referenceTargetOpt = referenceEntryOpt.value().getReferenceTarget();
                        referenceTargetOpt.has_value()) {
                        if (const auto& [targetType, targetString] = referenceTargetOpt.value();
                            symbolMapping.contains(targetType)) {
                            if (const auto targetEntryOpt = findMostCommonPathSymbol(
                                tagFileHandle,
                                targetString,
                                referencePath
                            ); targetEntryOpt.has_value()) {
                                if (const auto endLineOpt = targetEntryOpt.value().getEndLine();
                                    endLineOpt.has_value()) {
                                    result.emplace_back(
                                        iconv::toPath(targetEntryOpt.value().file),
                                        targetEntryOpt.value().name,
                                        symbolMapping.at(targetType),
                                        static_cast<uint32_t>(targetEntryOpt.value().address.lineNumber - 1),
                                        endLineOpt.value() - 1
                                    );
                                } else {
                                    logger::warn(format(
                                        "No endLine for '{}'", targetString
                                    ));
                                }
                            } else {
                                logger::warn(format(
                                    "No entry for '{}' in TypeRef '{}'", targetString, typeReferenceString
                                ));
                            }
                        } else {
                            logger::warn(format(
                                "Unknown targetType '{}' for TypeRef '{}'", targetType, typeReferenceString
                            ));
                        }
                    } else {
                        logger::warn(format(
                            "No reference target found for '{}'", typeReferenceString
                        ));
                    }
                } else {
                    logger::warn(format(
                        "No entry for '{}'", typeReferenceString
                    ));
                }
            } catch (exception& e) {
                logger::warn(format("Exception when getting info of symbol '{}': {}", typeReferenceString, e.what()));
            }
        }
    }
    if (full) {
        const auto tagFilePath = MemoryManipulator::GetInstance()->getProjectDirectory()
                                 / _tagFilenameMap.at(TagFileType::Function).first;
        if (!exists(tagFilePath)) {
            return result;
        }

        shared_lock lock{_functionTagFileMutex};
        const shared_ptr<tagFile> tagFileHandle(tagsOpen(tagFilePath.generic_string().c_str(), nullptr), tagsClose);
        if (tagFileHandle == nullptr) {
            logger::warn(format("Failed to open '{}'", tagFilePath.generic_string()));
            return result;
        }
        for (const auto& unknownString: collectSymbols(content).unknown) {
            try {
                if (const auto unknownEntryOpt = findMostCommonPathSymbol(
                    tagFileHandle,
                    unknownString,
                    referencePath
                ); unknownEntryOpt.has_value()) {
                    if (const auto& unknownEntry = unknownEntryOpt.value();
                        symbolMapping.contains(unknownEntry.kind)) {
                        if (const auto endLineOpt = unknownEntry.getEndLine();
                            endLineOpt.has_value()) {
                            result.emplace_back(
                                iconv::toPath(unknownEntry.file),
                                unknownEntry.name,
                                symbolMapping.at(unknownEntry.kind),
                                static_cast<uint32_t>(unknownEntry.address.lineNumber - 1),
                                endLineOpt.value() - 1
                            );
                        } else {
                            logger::warn(format(
                                "No endLine for '{}'", unknownEntry.name
                            ));
                        }
                    } else {
                        logger::warn(format(
                            "Unknown typeAlias '{}' for TypeRef '{}'", unknownEntry.kind, unknownString
                        ));
                    }
                } else {
                    logger::warn(format(
                        "No entry for '{}'", unknownString
                    ));
                }
            } catch (exception& e) {
                logger::warn(format("Exception when getting info of symbol '{}': {}", unknownString, e.what()));
            }
        }
    }
    return result;
}

void SymbolManager::updateRootPath(const filesystem::path& currentFilePath) {
    _tagFileNeedUpdateMap.at(TagFileType::Function) = true;
    _tagFileNeedUpdateMap.at(TagFileType::Structure) = true;
    thread([this, originalPath = absolute(currentFilePath).lexically_normal()] {
        auto tempPath = originalPath;
        while (tempPath != tempPath.parent_path()) {
            if (ranges::any_of(modulePaths, [&tempPath](const auto& modulePath) {
                return exists(tempPath / modulePath);
            })) {
                bool isSameRoot; {
                    shared_lock lock{_rootPathMutex};
                    isSameRoot = tempPath == _rootPath;
                }
                if (!isSameRoot) {
                    logger::info(format("Root path updated to Comware style: '{}'", tempPath.generic_string()));
                    unique_lock lock{_rootPathMutex};
                    _rootPath = tempPath;
                }
                return;
            }
            tempPath = tempPath.parent_path();
        }
        tempPath = originalPath;
        while (tempPath != tempPath.parent_path()) {
            if (exists(tempPath / "src")) {
                bool isSameRoot; {
                    shared_lock lock{_rootPathMutex};
                    isSameRoot = tempPath == _rootPath;
                }
                if (!isSameRoot) {
                    logger::info(format("Root path updated to normal style: '{}'", tempPath.generic_string()));
                    unique_lock lock{_rootPathMutex};
                    _rootPath = tempPath;
                }
                return;
            }
            tempPath = tempPath.parent_path();
        }
    }).detach();
}

unordered_map<string, ReviewReference> SymbolManager::_getReferences(
    const string& content,
    const filesystem::path& referencePath,
    const uint32_t depth
) const {
    const auto symbols = getSymbols(content, referencePath, true);
    mutex reviewReferencesMutex;
    unordered_map<string, ReviewReference> reviewReferences;
    vector<thread> retrieveContentThreads;
    reviewReferences.reserve(symbols.size());
    retrieveContentThreads.reserve(symbols.size());

    for (const auto& [path, name, type, startLine, endLine]: symbols) {
        if (ranges::any_of(excludePatterns, [&path](const auto& pattern) {
            return path.generic_string().contains(pattern);
        })) {
            continue;
        }

        retrieveContentThreads.emplace_back([&] {
            auto reviewReference = ReviewReference{
                path,
                name,
                iconv::autoDecode(fs::readFile(
                    path.generic_string(),
                    startLine,
                    endLine
                )),
                type,
                startLine,
                endLine,
                depth
            };

            unique_lock lock{reviewReferencesMutex};
            reviewReferences.emplace(format("{}:{}", path.generic_string(), name), move(reviewReference));
        });
    }

    for (auto& thread: retrieveContentThreads) {
        thread.join();
    }

    return reviewReferences;
}

void SymbolManager::_threadUpdateFunctionTagFile() {
    thread([this] {
        while (_isRunning) {
            _updateTagFile(TagFileType::Function);
            this_thread::sleep_for(1s);
        }
    }).detach();
}

void SymbolManager::_threadUpdateStructureTagFile() {
    thread([this] {
        while (_isRunning) {
            _updateTagFile(TagFileType::Structure);
            this_thread::sleep_for(1s);
        }
    }).detach();
}

void SymbolManager::_updateTagFile(const TagFileType tagFileType) {
    if (_tagFileNeedUpdateMap[tagFileType]) {
        const auto currentProjectDirectory = MemoryManipulator::GetInstance()->getProjectDirectory();
        const auto tagFilePath = currentProjectDirectory / _tagFilenameMap.at(tagFileType).first;
        const auto tempTagFilePath = currentProjectDirectory / _tagFilenameMap.at(tagFileType).second;
        string arguments; {
            shared_lock lock{_rootPathMutex};
            if (_rootPath.empty() || !exists(_rootPath)) {
                _tagFileNeedUpdateMap[tagFileType] = false;
                return;
            }
            arguments = format(
                R"(--excmd=combine -f "{}" --fields=+e+n --kinds-c={} --languages=C -R {})",
                tempTagFilePath.generic_string(),
                _tagKindsMap.at(tagFileType),
                _rootPath.generic_string()
            );
        }
        try {
            if (exists(tempTagFilePath)) {
                remove(tempTagFilePath);
            }
            system::runCommand("ctags.exe", arguments);
            unique_lock lock{tagFileType == TagFileType::Function ? _functionTagFileMutex : _structureTagFileMutex};
            rename(
                tempTagFilePath,
                tagFilePath
            );
        } catch (exception& e) {
            logger::warn(format("Exception when updating tags: {}", e.what()));
        }
        _tagFileNeedUpdateMap[tagFileType] = false;
    }
}
