#include <components/MemoryManipulator.h>
#include <components/WindowManager.h>
#include <types/EditedCompletion.h>
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
    for ([[maybe_unused]] const auto _: _completion | views::split("\r\n"sv)) {
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
               ? chrono::high_resolution_clock::now() - _reactTime.value() >= chrono::seconds(10)
               : false;
}

void EditedCompletion::addLine(const uint32_t line) {
    for (auto& reference: _references) {
        if (line < reference) {
            ++reference;
        }
    }
}

void EditedCompletion::removeLine(const uint32_t line) {
    for (auto& reference: _references) {
        if (line <= reference) {
            --reference;
        }
    }
}

CompletionEditClientMessage EditedCompletion::parse() {
    const auto memoryManipulator = MemoryManipulator::GetInstance();
    string currentContent;
    uint32_t count{};

    WindowManager::GetInstance()->sendF13();
    if (const auto fileHandleOpt = WindowManager::GetInstance()->getAssosiatedFileHandle(_windowHandle);
        fileHandleOpt.has_value()) {
        for (auto line = _references.front(); line <= _references.back(); ++line) {
            const auto currentLine = memoryManipulator->getLineContent(fileHandleOpt.value(), line);
            currentContent.append(currentLine).append("\r\n");
        }
        if (_isAccept) {
            auto reference = _references.begin();
            for (const auto originalRange: _completion | views::split("\r\n"sv)) {
                if (memoryManipulator->getLineContent(fileHandleOpt.value(), *reference).contains(
                    string{originalRange.begin(), originalRange.end()}
                )) {
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
            "Window handle {:#x} is invalid, skip calculate kept line count ", _windowHandle
        ));
    }
    return CompletionEditClientMessage(
        actionId,
        count,
        currentContent,
        _isAccept ? CompletionEditClientMessage::KeptRatio::All : CompletionEditClientMessage::KeptRatio::None
    );
}
