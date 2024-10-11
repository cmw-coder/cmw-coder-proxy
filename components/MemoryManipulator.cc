#include <components/MemoryManipulator.h>
#include <components/WindowManager.h>
#include <types/AddressToFunction.h>
#include <types/ConstMap.h>
#include <utils/fs.h>
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
                                            {0x08DB9B, 0x000001},
                                            {0x08F533},
                                            {0x0C93D8, 0x000010, 0x000000},
                                            {0x08D530},
                                            {0x08D474},
                                        },
                                        {
                                            0x1C7C00, 0x1C9740,
                                        },
                                        {
                                            {0x0FB214},
                                            {0x0FB25F},
                                            {0x0B1D87},
                                            {0x0FD8CF, 0x000000, 0x000000},
                                            {0x111123, 0x000000},
                                            {0x001F9D},
                                            {0x0810F7, 0x00000C},
                                            {0x115904},
                                            {0x0C62ED},
                                            {0x0C66C9},
                                            {0x0FB90B},
                                            {0x0D26D5},
                                            {0x0DA83D, 0x000000, 0x000001},
                                        },
                                        {
                                            0x1CCD44,
                                            {
                                                {0x1E3B84},
                                                {0x1BE0CC},
                                                {0x1C574C},
                                                {0x1E3B9C},
                                                {0x1E3BA4},
                                            },
                                            {0x1E535C},
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
                                            0x28C5C0, 0x28A920,
                                        },
                                        {},
                                        {
                                            0x288F30,
                                            {
                                                {0x26DAD0},
                                                {0x25A9B4},
                                                {0x28A0FC},
                                                {0x26DAE0},
                                                {0x26DAE8}
                                            },
                                            {0x26D5C8},
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
    if (const auto fileHandle = getHandle(MemoryAddress::HandleType::File)) {
        AddressToFunction<void(uint32_t, uint32_t, uint32_t)>(
            memory::offset(_memoryAddress.file.funcDelBufLine.base)
        )(fileHandle, line, _memoryAddress.file.funcDelBufLine.param3);
    }
}

void MemoryManipulator::freeSymbolListHandle(SymbolList* const symbolListHandle) const {
    AddressToFunction<int(SymbolListHandle)>(
        memory::offset(_memoryAddress.symbol.funcDestroySymbolList.base)
    )(symbolListHandle);
}

CaretDimension MemoryManipulator::getCaretDimension() const {
    return getCaretDimension(getCaretPosition().line);
}

CaretDimension MemoryManipulator::getCaretDimension(const uint32_t line) const {
    uint32_t height{}, xPosition{}, yPosition{};
    if (const auto windowHandle = getHandle(MemoryAddress::HandleType::Window)) {
        {
            yPosition = AddressToFunction<uint32_t(uint32_t, uint32_t)>(
                memory::offset(_memoryAddress.window.funcYPosFromLine.base)
            )(windowHandle, line);
        }

        memory::read(windowHandle + _memoryAddress.window.dataXPos.offset1, xPosition);
        memory::read(memory::offset(_memoryAddress.window.dataYDim.base), height);
    }
    return {height, xPosition, yPosition};
}

CaretPosition MemoryManipulator::getCaretPosition() const {
    const auto [_0, lineBegin, characterBegin, _1, _2] = _memoryAddress.window.dataSelection;
    CaretPosition cursorPosition{};
    memory::read(memory::offset(lineBegin.base), cursorPosition.line);
    memory::read(memory::offset(characterBegin.base), cursorPosition.character);
    return cursorPosition;
}

SymbolListHandle MemoryManipulator::getChildSymbolListHandle(SymbolName symbolName) const {
    SymbolBuffer symbolBuffer;

    auto symbolListHandle = AddressToFunction<SymbolListHandle()>(
        memory::offset(_memoryAddress.symbol.funcCreateSymbolList.base)
    )();

    AddressToFunction<void(void*)>(
        memory::offset(_memoryAddress.symbol.funcInitializeSymbolBuffer.base)
    )(symbolBuffer.data());

    AddressToFunction<void(const void*, SymbolListHandle, uint32_t, uint32_t, void*)>(
        memory::offset(_memoryAddress.symbol.funcGetSymbolChildren.base)
    )(
        symbolName.data(),
        symbolListHandle,
        _memoryAddress.symbol.funcGetSymbolChildren.param3,
        _memoryAddress.symbol.funcGetSymbolChildren.param4,
        symbolBuffer.data()
    );

    AddressToFunction<void(SymbolListHandle*)>(
        memory::offset(_memoryAddress.symbol.funcInitializeSymbolList.base)
    )(&symbolListHandle);

    return symbolListHandle;
}

filesystem::path MemoryManipulator::getCurrentFilePath() const {
    if (const auto fileHandle = getHandle(MemoryAddress::HandleType::File)) {
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

uint32_t MemoryManipulator::getHandle(const MemoryAddress::HandleType handleType) const {
    uint32_t address{}, handle{};
    if (!WindowManager::GetInstance()->getCurrentWindowHandle().has_value()) {
        return handle;
    }
    switch (handleType) {
        case MemoryAddress::HandleType::File: {
            address = memory::offset(_memoryAddress.file.handle);
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

string MemoryManipulator::getLineContent(const uint32_t handle, const uint32_t line) const {
    if (handle) {
        SimpleString payload;
        AddressToFunction<void(uint32_t, uint32_t, void*)>(
            memory::offset(_memoryAddress.file.funcGetBufLine.base)
        )(handle, line, payload.data());
        return payload.str();
    }
    return {};
}

filesystem::path MemoryManipulator::getProjectDirectory() const {
    char tempBuffer[256]{};
    memory::read(memory::offset(_memoryAddress.project.projectPath), tempBuffer);
    const auto filePath = filesystem::path(string(tempBuffer));
    return is_directory(filePath) ? filePath : filePath.parent_path();
}

optional<Selection> MemoryManipulator::getSelection() const {
    const auto [isSelecting, lineStart, characterStart, lineEnd, characterEnd] = _memoryAddress.window.dataSelection;
    bool isSelection;
    memory::read(memory::offset(isSelecting.base), isSelection);
    if (isSelection) {
        CaretPosition start{}, end{};
        memory::read(memory::offset(lineStart.base), start.line);
        memory::read(memory::offset(characterStart.base), start.character);
        memory::read(memory::offset(lineEnd.base), end.line);
        memory::read(memory::offset(characterEnd.base), end.character);
        return Selection(start, end);
    }
    return nullopt;
}

optional<SymbolName> MemoryManipulator::getSymbolName() const {
    return getSymbolName(getCaretPosition().line);
}

optional<SymbolName> MemoryManipulator::getSymbolName(const uint32_t line) const {
    if (const auto fileHandle = getHandle(MemoryAddress::HandleType::File)) {
        uint32_t symbolHandle{};
        int32_t symbolIndex{};
        SymbolName symbolName;

        AddressToFunction<void(uint32_t, uint32_t, uint32_t*, int32_t*, uint32_t)>(
            memory::offset(_memoryAddress.symbol.funcGetSymbolHandle.base)
        )(fileHandle, line, &symbolHandle, &symbolIndex, _memoryAddress.symbol.funcGetSymbolHandle.param5);
        if (symbolIndex < 0) {
            return nullopt;
        }

        AddressToFunction<int(uint32_t, uint32_t, void*)>(
            memory::offset(_memoryAddress.symbol.funcGetSymbolNameByLine.base)
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

optional<SymbolName> MemoryManipulator::getSymbolName(SymbolRecord symbolRecord) const {
    SimpleString errorMessage;
    SymbolName symbolName;

    if (AddressToFunction<bool(void*, void*, const void*)>(
        memory::offset(_memoryAddress.symbol.funcGetSymbolNameByRecord.base)
    )(errorMessage.data(), symbolName.data(), symbolRecord.data())) {
        return symbolName;
    }
    return nullopt;
}

optional<SymbolName> MemoryManipulator::getSymbolName(SymbolList* const symbolListHandle, const uint32_t index) const {
    if (index < symbolListHandle->size()) {
        SymbolName symbolName{};

        AddressToFunction<void(uint32_t, void*)>(
            memory::offset(_memoryAddress.symbol.funcGetSymbolNameByAddress.base)
        )(
            AddressToFunction<uint32_t(SymbolListHandle, uint32_t)>(
                memory::offset(_memoryAddress.symbol.funcGetSymbolListNameAddress.base)
            )(symbolListHandle, index) + _memoryAddress.symbol.funcGetSymbolNameByAddress.param1Offset1,
            symbolName.data()
        );

        return symbolName;
    }

    return nullopt;
}

optional<SymbolRecord> MemoryManipulator::getSymbolRecord(SymbolName symbolName) const {
    SymbolRecord symbolRecord;
    if (!symbolName.depth() || symbolName.depth() > 127) {
        return nullopt;
    }
    try {
        if (
            const auto result = AddressToFunction<char*(void*, void*)>(
                memory::offset(_memoryAddress.symbol.funcGetSymbolRecord.base)
            )(symbolName.data(), symbolRecord.data());
            stoi(result) >= 0
        ) {
            return symbolRecord;
        }
    } catch (invalid_argument&) {}
    return nullopt;
}

optional<SymbolRecord> MemoryManipulator::getSymbolRecordDeclared(SymbolName symbolName) const {
    if (AddressToFunction<bool(void*, uint32_t, uint32_t)>(
        memory::offset(_memoryAddress.symbol.funcTransformSymbolNameToDeclaredType.base)
    )(
        symbolName.data(),
        _memoryAddress.symbol.funcTransformSymbolNameToDeclaredType.param2,
        _memoryAddress.symbol.funcTransformSymbolNameToDeclaredType.param3
    )) {
        return getSymbolRecord(symbolName);
    }
    return nullopt;
}

void MemoryManipulator::setCaretPosition(const CaretPosition& caretPosition) const {
    if (const auto windowHandle = getHandle(MemoryAddress::HandleType::Window)) {
        AddressToFunction<void(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)>(
            memory::offset(_memoryAddress.window.funcSetWndSel.base)
        )(windowHandle, caretPosition.line, caretPosition.character, caretPosition.line, caretPosition.character);
    }
}

void MemoryManipulator::setLineContent(const uint32_t line, const string& content, const bool isInsertion) const {
    if (const auto fileHandle = getHandle(MemoryAddress::HandleType::File)) {
        AddressToFunction<void(uint32_t, uint32_t, void*)>(memory::offset(
            isInsertion ? _memoryAddress.file.funcInsBufLine.base : _memoryAddress.file.funcPutBufLine.base
        ))(fileHandle, line, SimpleString(content).data());
    }
}

void MemoryManipulator::setSelectionContent(const string& content) const {
    if (WindowManager::GetInstance()->getCurrentWindowHandle().has_value()) {
        uint32_t param1;
        memory::read(memory::offset(_memoryAddress.window.funcSetBufSelText.param1Base), param1);
        AddressToFunction<void(uint32_t, const char*)>(
            memory::offset(_memoryAddress.window.funcSetBufSelText.base)
        )(param1, content.c_str());
    }
}
