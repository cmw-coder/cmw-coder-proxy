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
using namespace types;
using namespace utils;

namespace {
    struct SymbolCollection {
        unordered_set<string> references, unknown;
    };

    const unordered_map<string, SymbolInfo::Type> symbolMapping =
    {
        {"d", SymbolInfo::Type::Macro},
        {"enum", SymbolInfo::Type::Enum},
        {"f", SymbolInfo::Type::Function},
        {"struct", SymbolInfo::Type::Struct},
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
        "ACCESS/src/sbin",
        "CRYPTO/src/sbin",
        "DC/src/sbin",
        "DEV/src/sbin",
        "DLP/src/sbin",
        "DPI/src/sbin",
        "DRV_SIMSWITCH/src/sbin",
        "DRV_SIMWARE9/src/sbin",
        "FE/src/sbin",
        "FW/src/sbin",
        "IP/src/sbin",
        "L2VPN/src/sbin",
        "LAN/src/sbin",
        "LB/src/sbin",
        "LINK/src/sbin",
        "LSM/src/sbin",
        "MCAST/src/sbin",
        "NETFWD/src/sbin",
        "OFP/src/sbin",
        "PSEC/src/sbin",
        "PUBLIC/include/comware",
        "QACL/src/sbin",
        "TEST/src/sbin",
        "VOICE/src/sbin",
        "VPN/src/sbin",
        "WLAN/src/sbin",
        "X86PLAT/src/sbin",
    };

    optional<uint32_t> getEndLine(const unordered_map<string, string>& symbolFields) {
        if (const auto key = "end";
            symbolFields.contains(key)) {
            return stoul(symbolFields.at(key));
        }
        return nullopt;
    }

    unordered_map<string, string> getSymbolFields(const decltype(tagEntry::fields)& fields) {
        unordered_map<string, string> symbolFields;
        for (auto index = 0; index < fields.count; ++index) {
            symbolFields.insert_or_assign((fields.list + index)->key, (fields.list + index)->value);
        }
        return symbolFields;
    }

    optional<pair<string, string>> getTypeReference(const unordered_map<string, string>& symbolFields) {
        if (const auto key = "end";
            symbolFields.contains(key)) {
            const auto value = symbolFields.at(key);
            if (const auto offset = value.find(':');
                offset != string::npos) {
                return make_pair(value.substr(0, offset), value.substr(offset + 1));
            }
        }
        return nullopt;
    }

    SymbolCollection collectSymbols(const string& prefixLines) {
        SymbolCollection result;
        vector<string> symbolList;
        copy(
            sregex_token_iterator(prefixLines.begin(), prefixLines.end(), symbolPattern),
            sregex_token_iterator(),
            back_inserter(symbolList)
        );
        for (const auto& symbol: symbolList) {
            if (symbol.length() < 2 || ignoredWords.contains(symbol)) {
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

vector<SymbolInfo> SymbolManager::getSymbols(const string& prefix, const bool full) const {
    vector<SymbolInfo> result;
    const auto tagsFilePath = MemoryManipulator::GetInstance()->getProjectDirectory() / _tagFile;
    if (!exists(tagsFilePath)) {
        return result;
    }
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
                    if (const auto& [typeName, reference] = typeReferenceOpt.value();
                        symbolMapping.contains(typeName) &&
                        tagsFind(tagsFileHandle, &entry, reference.c_str(), TAG_OBSERVECASE) == TagSuccess
                    ) {
                        if (const auto endLineOpt = getEndLine(getSymbolFields(entry.fields));
                            endLineOpt.has_value()) {
                            result.emplace_back(
                                iconv::toPath(entry.file),
                                entry.name,
                                symbolMapping.at(typeName),
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
    if (full) {
        for (const auto& symbol: collectSymbols(prefix).unknown) {
            try {
                tagEntry entry;
                if (tagsFind(tagsFileHandle, &entry, symbol.c_str(), TAG_OBSERVECASE) == TagSuccess &&
                    symbolMapping.contains(entry.kind)) {
                    if (const auto endLineOpt = getEndLine(getSymbolFields(entry.fields));
                        endLineOpt.has_value()) {
                        result.emplace_back(
                            iconv::toPath(entry.file),
                            entry.name,
                            symbolMapping.at(entry.kind),
                            static_cast<uint32_t>(entry.address.lineNumber - 1),
                            endLineOpt.value() - 1
                        );
                    }
                }
            } catch (exception& e) {
                logger::warn(format("Exception when getting info of symbol '{}': {}", symbol, e.what()));
            }
        }
    }
    tagsClose(tagsFileHandle);
    return result;
}

void SymbolManager::updateRootPath(const std::filesystem::path& currentFilePath) {
    _needUpdateTags.store(true);
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

void SymbolManager::_threadUpdateTags() {
    thread([this] {
        while (_isRunning) {
            if (_needUpdateTags.load()) {
                string arguments; {
                    shared_lock lock{_rootPathMutex};
                    if (_rootPath.empty() || !exists(_rootPath)) {
                        _needUpdateTags.store(false);
                        continue;
                    }
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
                _needUpdateTags.store(false);
            }
            this_thread::sleep_for(1s);
        }
    }).detach();
}
