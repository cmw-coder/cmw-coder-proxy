#include <components/Configurator.h>
#include <components/MemoryManipulator.h>
#include <components/WindowManager.h>
#include <types/AddressToFunction.h>
#include <types/ConstMap.h>
#include <utils/logger.h>
#include <utils/memory.h>

#include <windows.h>

using namespace components;
using namespace magic_enum;
using namespace models;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    constexpr SiVersion::Map<MemoryAddress> addressMap = {
        {
            {
                {
                    SiVersion::Major::V35, {
                        {
                            {
                                {
                                    SiVersion::Minor::V0086, {
                                        {
                                            0x1C35A4,
                                            {0x08DB9B},
                                            {0x08F533},
                                            {0x0C93D8, 0x000010, 0x000000},
                                            {0x08D530},
                                            {0x08D474},
                                        },
                                        {
                                            0x1C7C00,
                                            {0x000000, 0x000044},
                                        },
                                        {
                                            {0x0B1D87},
                                            {0x111123},
                                            {0x115904},
                                            {0x0C66C9},
                                        },
                                        {
                                            0x1CCD44,
                                            {0x1BE0CC, 0x1C574C, 0x1E3B9C, 0x1E3BA4},
                                            {0x000024},
                                            {0x1CCD3C},
                                            {0x09180C, 0X1CCD48},
                                            {0x108894},
                                            {0x1007D7},
                                        }
                                    }
                                }
                            }
                        }
                    }
                },
                {
                    SiVersion::Major::V40, {
                        {
                            {
                                {
                                    SiVersion::Minor::V0128,
                                    {
                                        {
                                            0x26D484,
                                            {0x08DB9B},
                                            {0x0C6660},
                                            {0x0419A0, 0x000010, 0x000000},
                                            {0x0C4A60},
                                            {0x0C4FB0},
                                        },
                                        {
                                            0x28C5C0,
                                            {0x000000, 0x000058},
                                        },
                                        {
                                            {},
                                            {},
                                            {},
                                            {},
                                        },
                                        {
                                            0x288F30,
                                            {0x25A9B4, 0x28A0FC, 0x26DAE0, 0x26DAE8},
                                            {0x000024},
                                            {0x288F48},
                                            {0x0C88C0, 0X288F2C},
                                            {0x187660},
                                            {0x179D30},
                                        }
                                    }
                                }
                            }
                        },
                    }
                }
            }
        }
    };
}

MemoryManipulator::MemoryManipulator(const SiVersion::Full version)
    : _memoryAddress(addressMap.at(version.first).at(version.second)),
      _processHandle(GetCurrentProcess(), CloseHandle) {
    logger::log("MemoryManipulator is initialized");
}

void MemoryManipulator::deleteLineContent(const uint32_t line) const {
    if (const auto fileHandle = _getHandle(MemoryAddress::HandleType::File)) {
        AddressToFunction<void(uint32_t, uint32_t, uint32_t)>(
            memory::offset(_memoryAddress.file.funcDelBufLine.base)
        )(fileHandle, line, 1);
    }
}

CaretDimension MemoryManipulator::getCaretDimension() const {
    return getCaretDimension(getCaretPosition().line);
}

CaretDimension MemoryManipulator::getCaretDimension(const uint32_t line) const {
    CaretDimension caretDimension{};
    if (const auto windowHandle = _getHandle(MemoryAddress::HandleType::Window)) {
        caretDimension.yPosition = AddressToFunction<uint32_t(uint32_t, uint32_t)>(
            memory::offset(_memoryAddress.window.funcYPosFromLine.base)
        )(windowHandle, line);

        memory::read(windowHandle + _memoryAddress.window.dataXPos.offset1, caretDimension.xPosition);
        memory::read(memory::offset(_memoryAddress.window.dataYDim.base), caretDimension.height);
    }
    return caretDimension;
}

CaretPosition MemoryManipulator::getCaretPosition() const {
    CaretPosition cursorPosition{};
    memory::read(memory::offset(_memoryAddress.window.dataSelection.lineStart.base), cursorPosition.line);
    memory::read(memory::offset(_memoryAddress.window.dataSelection.characterStart.base),
                 cursorPosition.character);
    return cursorPosition;
}

string MemoryManipulator::getFileName() const {
    if (const auto fileHandle = _getHandle(MemoryAddress::HandleType::File)) {
        uint32_t param1Base, param1;
        SimpleString payload;

        memory::read(fileHandle + _memoryAddress.file.funcGetBufName.param1Base, param1Base);
        memory::read(param1Base + _memoryAddress.file.funcGetBufName.param1Offset1, param1);
        AddressToFunction<void(uint32_t, void*)>(
            memory::offset(_memoryAddress.file.funcGetBufName.base)
        )(param1, payload.data());
        return payload.str();
    }
    return {};
}

string MemoryManipulator::getLineContent(const uint32_t line) const {
    if (const auto fileHandle = _getHandle(MemoryAddress::HandleType::File)) {
        SimpleString payload;
        AddressToFunction<void(uint32_t, uint32_t, void*)>(
            memory::offset(_memoryAddress.file.funcGetBufLine.base)
        )(fileHandle, line, payload.data());
        return payload.str();
    }
    return {};
}

string MemoryManipulator::getProjectDirectory() const {
    if (const auto projectHandle = _getHandle(MemoryAddress::HandleType::Project)) {
        char tempBuffer[256];
        uint32_t offset1{};
        if (Configurator::GetInstance()->version().first == SiVersion::Major::V35) {
            memory::read(projectHandle + _memoryAddress.project.dataProjDir.offset1, offset1);
            memory::read(offset1 + _memoryAddress.project.dataProjDir.offset2, tempBuffer);
        } else {
            memory::read(projectHandle + _memoryAddress.project.dataProjDir.offset2, tempBuffer);
        }
        logger::info(format("Current Project: '{}'", tempBuffer));
        return {tempBuffer};
    }
    return {};
}

optional<SymbolName> MemoryManipulator::getSymbolName() const {
    return getSymbolName(getCaretPosition().line);
}

optional<SymbolName> MemoryManipulator::getSymbolName(const uint32_t line) const {
    if (const auto fileHandle = _getHandle(MemoryAddress::HandleType::File)) {
        uint32_t symbolHandle{};
        int32_t symbolIndex{};
        SymbolName symbolName;

        AddressToFunction<void(uint32_t, uint32_t, uint32_t*, int32_t*, uint32_t)>(
            memory::offset(_memoryAddress.symbol.funcGetSymbolHandle.base)
        )(fileHandle, line, &symbolHandle, &symbolIndex, 0);
        if (symbolIndex < 0) {
            return nullopt;
        }

        AddressToFunction<int(uint32_t, uint32_t, void*)>(
            memory::offset(_memoryAddress.symbol.funcGetSymbolName.base)
        )(
            symbolHandle,
            AddressToFunction<uint32_t(uint32_t, int32_t)>(
                memory::offset(_memoryAddress.symbol.funcGetSymbolAddress.base)
            )(symbolHandle, symbolIndex),
            symbolName.data()
        );
        return symbolName;
    }

    return nullopt;
}

optional<SymbolRecord> MemoryManipulator::getSymbolRecord(SymbolName symbolName) const {
    SymbolRecord symbolRecord;
    if (const auto result = AddressToFunction<char*(void*, void*)>(
            memory::offset(_memoryAddress.symbol.funcGetSymbolRecord.base)
        )(symbolName.data(), symbolRecord.data());
        stoi(result) >= 0) {
        return symbolRecord;
    }
    return nullopt;
}

void MemoryManipulator::setCaretPosition(const CaretPosition& caretPosition) const {
    if (const auto windowHandle = _getHandle(MemoryAddress::HandleType::Window)) {
        AddressToFunction<void(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)>(
            memory::offset(_memoryAddress.window.funcSetWndSel.base)
        )(windowHandle, caretPosition.line, caretPosition.character, caretPosition.line, caretPosition.character);
    }
}

void MemoryManipulator::setLineContent(const uint32_t line, const string& content, const bool isInsertion) const {
    if (const auto fileHandle = _getHandle(MemoryAddress::HandleType::File)) {
        AddressToFunction<void(uint32_t, uint32_t, void*)>(memory::offset(
            isInsertion ? _memoryAddress.file.funcInsBufLine.base : _memoryAddress.file.funcPutBufLine.base
        ))(fileHandle, line, SimpleString(content).data());
    }
}

void MemoryManipulator::setSelectionContent(const string& content) const {
    if (WindowManager::GetInstance()->hasValidCodeWindow()) {
        uint32_t param1;
        memory::read(memory::offset(_memoryAddress.window.funcSetBufSelText.param1Base), param1);
        AddressToFunction<void(uint32_t, const char*)>(
            memory::offset(_memoryAddress.window.funcSetBufSelText.base)
        )(param1, content.c_str());
    }
}

uint32_t MemoryManipulator::_getHandle(const MemoryAddress::HandleType handleType) const {
    uint32_t address{}, handle{};
    if (!WindowManager::GetInstance()->hasValidCodeWindow()) {
        return handle;
    }
    switch (handleType) {
        case MemoryAddress::HandleType::File: {
            address = memory::offset(_memoryAddress.file.handle);
            break;
        }
        case MemoryAddress::HandleType::Project: {
            address = memory::offset(_memoryAddress.project.handle);
            break;
        }
        case MemoryAddress::HandleType::Window: {
            address = memory::offset(_memoryAddress.window.handle);
            break;
        }
    }
    memory::read(address, handle);
    return handle;
}
