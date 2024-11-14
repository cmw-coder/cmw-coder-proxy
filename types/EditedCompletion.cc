#include <components/MemoryManipulator.h>
#include <components/WindowManager.h>
#include <types/EditedCompletion.h>
#include <utils/iconv.h>
#include <utils/logger.h>

using namespace components;
using namespace models;
using namespace std;
using namespace types;
using namespace utils;

EditedCompletion::EditedCompletion(
    string actionId,
    const uint32_t windowHandle,
    const uint32_t line,
    string completion
) : actionId(move(actionId)), _windowHandle(windowHandle), _completion(move(completion)) {
    for ([[maybe_unused]] const auto _: _completion | views::split("\n"sv)) {
        if (_references.empty()) {
            _references.emplace_back(line);
        } else {
            _references.emplace_back(_references.back() + 1);
        }
    }
}

void EditedCompletion::react(const bool isAccept) {
    _isAccept = isAccept;
    _reactTime = chrono::high_resolution_clock::now();
}

bool EditedCompletion::canReport() const {
    return _reactTime.has_value()
               ? chrono::high_resolution_clock::now() - _reactTime.value() >= 1min
               : false;
}

void EditedCompletion::addLine(const uint32_t startLine, const uint32_t count) {
    for (auto& reference: _references) {
        if (startLine <= reference) {
            reference += count;
        }
    }
}

void EditedCompletion::removeLine(const uint32_t startLine, const uint32_t count) {
    for (auto& reference: _references) {
        if (startLine <= reference) {
            reference -= count;
        }
    }
}

CompletionEditClientMessage EditedCompletion::parse() const {
    const auto memoryManipulator = MemoryManipulator::GetInstance();
    string currentContent;
    uint32_t count{};

    if (const auto fileHandleOpt = WindowManager::GetInstance()->getAssociatedFileHandle(_windowHandle);
        fileHandleOpt.has_value() && !_references.empty()) {
        WindowManager::GetInstance()->sendF13();
        for (uint32_t line = _references.front() < 10 ? 0 : _references.front() - 10;
             line <= _references.back() + 10; ++line) {
            currentContent
                    .append(iconv::autoDecode(memoryManipulator->getLineContent(fileHandleOpt.value(), line)))
                    .append("\n");
        }
        if (_isAccept) {
            auto reference = _references.begin();
            for (const auto originalRange: _completion | views::split("\n"sv)) {
                if (iconv::autoDecode(
                    memoryManipulator->getLineContent(fileHandleOpt.value(), *reference)
                ).contains(string{originalRange.begin(), originalRange.end()})) {
                    ++count;
                }
                ++reference;
            }
        } else {
            count = _references.back() - _references.front() + 1;
        }
    } else {
        // TODO: Use file read method
        logger::info(format(
            "Window handle {:#x} is invalid, skip parsing EditedCompletion.", _windowHandle
        ));
    }
    return CompletionEditClientMessage(
        actionId,
        count,
        currentContent,
        _isAccept ? CompletionEditClientMessage::KeptRatio::All : CompletionEditClientMessage::KeptRatio::None
    );
}
