#include <magic_enum/magic_enum.hpp>

#include <types/CompletionComponents.h>
#include <utils/base64.h>
#include <utils/iconv.h>

using namespace magic_enum;
using namespace models;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    uint64_t getMilliseconds(const chrono::time_point<chrono::system_clock>& time = chrono::system_clock::now()) {
        return duration_cast<chrono::milliseconds>(time.time_since_epoch()).count();
    }
}

CompletionComponents::CompletionComponents(
    const GenerateType generateType,
    const CaretPosition& caretPosition,
    const filesystem::path& path
): path(path), _generateType(generateType), _caretPosition(caretPosition) {
    _resetTimePoints();
}

string CompletionComponents::getPrefix() const {
    return _prefix;
}

vector<filesystem::path> CompletionComponents::getRecentFiles() const {
    return _recentFiles;
}

string CompletionComponents::getSuffix() const {
    return _suffix;
}

bool CompletionComponents::needCache(const CaretPosition& caretPosition) const {
    return caretPosition.line == _caretPosition.line;
}

void CompletionComponents::setContext(
    const string& prefix,
    const string& infix,
    const string& suffix
) {
    _prefix = prefix;
    _infix = infix;
    _suffix = suffix;
    _contextTime = chrono::system_clock::now();
}

void CompletionComponents::setRecentFiles(const vector<filesystem::path>& recentFiles) {
    _recentFiles = recentFiles;
    _recentFilesTime = chrono::system_clock::now();
}

void CompletionComponents::setSymbols(const vector<SymbolInfo>& symbols) {
    _symbols = symbols;
    _symbolTime = chrono::system_clock::now();
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
        {"path", iconv::autoDecode(path.generic_string())},
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

void CompletionComponents::updateCaretPosition(const CaretPosition& caretPosition) {
    _caretPosition = caretPosition;
}

void CompletionComponents::useCachedContext(
    const string& currentLinePrefix,
    const string& newInfix,
    const string& currentLineSuffix
) {
    if (const auto lastNewLineInCachedPrefix = _prefix.find_last_of('\n');
        lastNewLineInCachedPrefix != string::npos) {
        _prefix = _prefix.substr(lastNewLineInCachedPrefix + 1) + currentLinePrefix;
    } else {
        _prefix = currentLinePrefix;
    }
    _infix = newInfix;
    if (const auto firstNewLineInSuffix = currentLineSuffix.find('\n');
        firstNewLineInSuffix != string::npos) {
        _suffix = currentLineSuffix + _suffix.substr(firstNewLineInSuffix);
    } else {
        _suffix = currentLineSuffix;
    }
    _resetTimePoints();
}

void CompletionComponents::_resetTimePoints() {
    const auto currentTime = chrono::system_clock::now();
    _initTime = currentTime;
    _contextTime = currentTime;
    _recentFilesTime = currentTime;
    _symbolTime = currentTime;
}
