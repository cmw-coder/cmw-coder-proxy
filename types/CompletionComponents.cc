#include <magic_enum/magic_enum.hpp>

#include <types/CompletionComponents.h>
#include <utils/base64.h>
#include <utils/iconv.h>

using namespace magic_enum;
using namespace std;
using namespace chrono;
using namespace types;
using namespace utils;

namespace {
    uint64_t getMilliseconds(const time_point<system_clock>& time = system_clock::now()) {
        return duration_cast<milliseconds>(time.time_since_epoch()).count();
    }
}

CompletionComponents::CompletionComponents(
    const GenerateType generateType,
    const CaretPosition& caretPosition,
    const filesystem::path& path
): _generateType(generateType), _caretPosition(caretPosition), _path(path) {
    const auto currentTime = system_clock::now();
    _initTime = currentTime;
    _contextTime = currentTime;
    _recentFilesTime = currentTime;
    _symbolTime = currentTime;
}

filesystem::path CompletionComponents::getPath() const {
    return _path;
}

void CompletionComponents::setContext(
    const string& prefix,
    const string& infix,
    const string& suffix
) {
    _prefix = prefix;
    _infix = infix;
    _suffix = suffix;
    _contextTime = system_clock::now();
}

void CompletionComponents::setRecentFiles(const vector<filesystem::path>& recentFiles) {
    _recentFiles = recentFiles;
    _recentFilesTime = system_clock::now();
}

void CompletionComponents::setSymbols(const vector<models::SymbolInfo>& symbols) {
    _symbols = symbols;
    _symbolTime = system_clock::now();
}

nlohmann::json CompletionComponents::toJson() const {
    nlohmann::json result = {
        {"type", enum_name(_generateType)},
        {
            "caret", {
                {"character", _caretPosition.character},
                {"line", _caretPosition.line},
            }
        },
        {"path", iconv::autoDecode(_path.generic_string())},
        {
            "context", {
                {"infix", base64::to_base64(_infix)},
                {"prefix", base64::to_base64(_prefix)},
                {"suffix", base64::to_base64(_suffix)},
            },
        },
        {"recentFiles", nlohmann::json::array()},
        {"symbols", nlohmann::json::array()},
        {
            "times", {
                {"start", getMilliseconds(_initTime)},
                {"context", getMilliseconds(_contextTime)},
                {"recentFiles", getMilliseconds(_recentFilesTime)},
                {"symbol", getMilliseconds(_symbolTime)},
                {"end", getMilliseconds()},
            }
        }
    };
    for (const auto& recentFile: _recentFiles) {
        result["recentFiles"].push_back(iconv::autoDecode(recentFile.generic_string()));
    }
    for (const auto& [path, name, type, startLine, endLine]: _symbols) {
        result["symbols"].push_back({
            {"endLine", endLine},
            {"name", name},
            {"path", iconv::autoDecode(path.generic_string())},
            {"startLine", startLine},
            {"type", enum_name(type)},
        });
    }
    return result;
}
