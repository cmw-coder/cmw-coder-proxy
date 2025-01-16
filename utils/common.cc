#include <components/ConfigManager.h>
#include <components/InteractionMonitor.h>
#include <components/MemoryManipulator.h>
#include <components/WindowManager.h>
#include <utils/common.h>

#include <combaseapi.h>

using namespace components;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    const std::unordered_map<SiVersion::Major, std::unordered_map<char, uint32_t>> keyMap = {
        {
            SiVersion::Major::V35,
            {
                {'S', 0x0053},
            }
        },
        {
            SiVersion::Major::V40,
            {
                {'S', 0x000053},
            }
        },
    };
    const std::unordered_map<SiVersion::Major, std::unordered_map<Modifier, uint32_t>> modifierMap = {
        {
            SiVersion::Major::V35,
            {
                {Modifier::Shift, 0x0300},
                {Modifier::Ctrl, 0x0400},
                {Modifier::Alt, 0x0800},
            }
        },
        {
            SiVersion::Major::V40,
            {
                {Modifier::Shift, 0x030000},
                {Modifier::Ctrl, 0x040000},
                {Modifier::Alt, 0x080000},
            }
        },
    };
}

bool common::checkHighestBit(const uint16_t value) noexcept {
    return value & 0b1000'0000'0000'0000;
}

uint32_t common::countLines(const std::string& content) {
    return content.empty() ? 0 : ranges::count(content, '\n') + 1;
}

CaretDimension common::getCaretDimensions(const bool waitTillAvailable) {
    const auto [clientX, clientY] = WindowManager::GetInstance()->getClientPosition();

    auto [height, xPosition, yPosition] = MemoryManipulator::GetInstance()->getCaretDimension();
    while (waitTillAvailable && !height) {
        std::this_thread::sleep_for(50ms);
        const auto [
            newHeight,
            newXPosition,
            newYPosition
        ] = MemoryManipulator::GetInstance()->getCaretDimension();
        height = newHeight;
        xPosition = newXPosition;
        yPosition = newYPosition;
    }

    return {
        height,
        clientX + xPosition,
        clientY + yPosition - 1,
    };
}

void common::insertContent(const std::string& content) { {
        const auto memoryManipulator = MemoryManipulator::GetInstance();
        const auto currentPosition = memoryManipulator->getCaretPosition();
        uint32_t insertedLineCount{0}, lastLineLength{0};
        const auto interactionLock = InteractionMonitor::GetInstance()->getInteractionLock();
        for (const auto lineRange: content | views::split("\n"sv)) {
            auto lineContent = string{lineRange.begin(), lineRange.end()};
            if (insertedLineCount == 0) {
                lastLineLength = currentPosition.character + 1 + lineContent.size();
                memoryManipulator->setSelectionContent(lineContent);
            } else {
                lastLineLength = lineContent.size();
                memoryManipulator->setLineContent(currentPosition.line + insertedLineCount, lineContent, true);
            }
            this_thread::sleep_for(5ms);
            ++insertedLineCount;
        }
        memoryManipulator->setCaretPosition({lastLineLength, currentPosition.line + insertedLineCount - 1});
    }
    if (ConfigManager::GetInstance()->version().first == SiVersion::Major::V35) {
        WindowManager::GetInstance()->sendLeftThenRight();
    } else {
        WindowManager::GetInstance()->sendEnd();
    }
}

void common::replaceContent(const Selection& replaceRange, string content) { {
        const auto memoryManipulator = MemoryManipulator::GetInstance();
        uint32_t insertedLineCount{0}, lastLineLength{0};

        const auto interactionLock = InteractionMonitor::GetInstance()->getInteractionLock();
        const auto lastLineContent = memoryManipulator->getLineContent(
            memoryManipulator->getHandle(models::MemoryAddress::HandleType::File), replaceRange.end.line
        );
        memoryManipulator->setSelection({replaceRange.begin, {lastLineContent.length(), replaceRange.end.line}});
        if (replaceRange.end.character < lastLineContent.length()) {
            content.append(lastLineContent.substr(replaceRange.end.character, lastLineContent.length()));
        }

        const auto lastNewLineIndex = content.find_last_of('\n');
        lastLineLength = content.length() - (lastNewLineIndex != string::npos ? lastNewLineIndex + 1 : 0);

        for (const auto lineRange: content | views::split("\n"sv)) {
            auto lineContent = string{lineRange.begin(), lineRange.end()};
            if (insertedLineCount == 0) {
                memoryManipulator->setSelectionContent(lineContent);
            } else {
                memoryManipulator->setLineContent(replaceRange.begin.line + insertedLineCount, lineContent, true);
            }
            this_thread::sleep_for(5ms);
            ++insertedLineCount;
        }

        memoryManipulator->setCaretPosition({lastLineLength, replaceRange.begin.line + insertedLineCount - 1});
    }
    if (ConfigManager::GetInstance()->version().first == SiVersion::Major::V35) {
        WindowManager::GetInstance()->sendLeftThenRight();
    } else {
        WindowManager::GetInstance()->sendEnd();
    }
}

uint32_t common::toKeycode(const char key, const Modifier modifier) {
    const auto majorVersion = ConfigManager::GetInstance()->version().first;
    try {
        return keyMap.at(majorVersion).at(key) +
               modifierMap.at(majorVersion).at(modifier);
    } catch (...) {
        return {};
    }
}

uint32_t common::toKeycode(const char key, const ModifierSet& modifiers) {
    const auto majorVersion = ConfigManager::GetInstance()->version().first;
    try {
        auto keycode = keyMap.at(majorVersion).at(key);
        for (const auto& modifier: modifiers) {
            keycode += modifierMap.at(majorVersion).at(modifier);
        }
        return keycode;
    } catch (...) {
        return {};
    }
}

string common::uuid() {
    GUID gidReference;
    HRESULT result;
    do {
        result = CoCreateGuid(&gidReference);
        if (result != S_OK) {
            std::this_thread::sleep_for(1ms);
        }
    } while (result != S_OK);
    return format(
        "{:08x}-{:04x}-{:04x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
        gidReference.Data1, gidReference.Data2, gidReference.Data3,
        gidReference.Data4[0], gidReference.Data4[1],
        gidReference.Data4[2], gidReference.Data4[3], gidReference.Data4[4],
        gidReference.Data4[5], gidReference.Data4[6], gidReference.Data4[7]
    );
}
