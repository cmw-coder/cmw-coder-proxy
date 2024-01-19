#include <format>
#include <regex>

#include <magic_enum.hpp>

#include <components/CompletionManager.h>
#include <components/Configurator.h>
#include <components/InteractionMonitor.h>
#include <components/WebsocketManager.h>
#include <components/WindowManager.h>
#include <types/common.h>
#include <types/CompactString.h>
#include <types/SiVersion.h>
#include <utils/logger.h>
#include <utils/memory.h>
#include <utils/system.h>
#include <utils/window.h>

#include <windows.h>

using namespace components;
using namespace magic_enum;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    void checkValidCodeWindow() {
        if (!WindowManager::GetInstance()->hasValidCodeWindow()) {
            throw runtime_error("No valid code window");
        }
    }

    constexpr auto debugLogKey = "CMWCODER_debugLog";
}

InteractionMonitor::InteractionMonitor()
    : _subKey(Configurator::GetInstance()->version().first == SiVersion::Major::V35
                  ? R"(SOFTWARE\Source Dynamics\Source Insight\3.0)"
                  : R"(SOFTWARE\Source Dynamics\Source Insight\4.0)"),
      _baseAddress(reinterpret_cast<uint32_t>(GetModuleHandle(nullptr))),
      _keyHelper(Configurator::GetInstance()->version().first),
      _keyHookHandle(
          SetWindowsHookEx(
              WH_KEYBOARD,
              _keyProcedureHook,
              GetModuleHandle(nullptr),
              GetCurrentThreadId()
          ),
          UnhookWindowsHookEx
      ),
      _mouseHookHandle(
          SetWindowsHookEx(
              WH_MOUSE,
              _mouseProcedureHook,
              GetModuleHandle(nullptr),
              GetCurrentThreadId()
          ),
          UnhookWindowsHookEx
      ),
      _processHandle(GetCurrentProcess(), CloseHandle),
      _windowHookHandle(
          SetWindowsHookEx(
              WH_CALLWNDPROC,
              _windowProcedureHook,
              GetModuleHandle(nullptr),
              GetCurrentThreadId()
          ),
          UnhookWindowsHookEx
      ) {
    if (!_processHandle) {
        logger::error("Failed to get Source Insight's process handle");
        abort();
    }
    if (!_windowHookHandle) {
        logger::error("Failed to set window hook.");
        abort();
    }
    try {
        _memoryAddress = memory::getAddresses(Configurator::GetInstance()->version());
    } catch (runtime_error& e) {
        logger::error(e.what());
        abort();
    }

    _monitorCaretPosition();
    _monitorDebugLog();
}

InteractionMonitor::~InteractionMonitor() {
    _isRunning.store(false);
}

void InteractionMonitor::deleteLineContent(const uint32_t line) const {
    const auto funcDelBufLine = StdCallFunction<void(uint32_t, uint32_t, uint32_t)>(
        _baseAddress + _memoryAddress.file.funcDelBufLine.base
    );

    if (const auto fileHandle = _getFileHandle()) {
        funcDelBufLine(fileHandle, line, 1);
    }
}

tuple<int64_t, int64_t> InteractionMonitor::getCaretPixels(const uint32_t line) const {
    const auto [clientX, clientY] = WindowManager::GetInstance()->getClientPosition();
    const auto funcYPosFromLine = StdCallFunction<uint32_t(uint32_t, uint32_t)>(
        _baseAddress + _memoryAddress.window.funcYPosFromLine.base
    );
    const auto windowHandle = _getWindowHandle();
    const auto yPos = funcYPosFromLine(windowHandle, line);

    uint32_t xPos, yDim;
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(windowHandle + _memoryAddress.window.dataXPos.offset1),
        &xPos,
        sizeof(xPos),
        nullptr
    );
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.window.dataYDim.base),
        &yDim,
        sizeof(yDim),
        nullptr
    );

    logger::debug(format(
        "Line {} Positions: Client ({}, {}), Caret ({}, {}, {})",
        line, clientX, clientY, xPos, yPos, yDim
    ));
    return {
        clientX + xPos,
        clientY + yPos + static_cast<int64_t>(round(0.625 * yDim)),
    };
}

CaretPosition InteractionMonitor::getCaretPosition() const {
    CaretPosition cursorPosition{};
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.window.dataSelection.lineStart.base),
        &cursorPosition.line,
        sizeof(cursorPosition.line),
        nullptr
    );
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.window.dataSelection.characterStart.base),
        &cursorPosition.character,
        sizeof(cursorPosition.character),
        nullptr
    );
    return cursorPosition;
}

string InteractionMonitor::getFileName() const {
    uint32_t param1;
    const auto functionGetBufName = StdCallFunction<void*(int, void*)>(
        _baseAddress + _memoryAddress.file.funcGetBufName.base
    ); {
        const auto fileHandle = _getFileHandle();
        uint32_t param1Base;
        ReadProcessMemory(
            _processHandle.get(),
            reinterpret_cast<LPCVOID>(fileHandle + _memoryAddress.file.funcGetBufName.param1Base),
            &param1Base,
            sizeof(param1Base),
            nullptr
        );
        ReadProcessMemory(
            _processHandle.get(),
            reinterpret_cast<LPCVOID>(param1Base + _memoryAddress.file.funcGetBufName.param1Offset1),
            &param1,
            sizeof(param1),
            nullptr
        );
    }

    CompactString payload;
    functionGetBufName(param1, payload.data());
    return payload.str();
}

string InteractionMonitor::getLineContent() const {
    return getLineContent(getCaretPosition().line);
}

string InteractionMonitor::getLineContent(const uint32_t line) const {
    const auto functionGetBufLine = StdCallFunction<void(uint32_t, uint32_t, void*)>(
        _baseAddress + _memoryAddress.file.funcGetBufLine.base
    );

    if (const auto fileHandle = _getFileHandle()) {
        CompactString payload;
        functionGetBufLine(fileHandle, line, payload.data());
        return payload.str();
    }
    return {};
}

string InteractionMonitor::getProjectDirectory() const {
    if (const auto projectHandle = _getProjectHandle()) {
        char tempBuffer[256];
        uint32_t offset1;

        ReadProcessMemory(
            _processHandle.get(),
            reinterpret_cast<LPCVOID>(projectHandle + _memoryAddress.project.dataProjDir.offset1),
            &offset1,
            sizeof(offset1),
            nullptr
        );
        ReadProcessMemory(
            _processHandle.get(),
            reinterpret_cast<LPCVOID>(offset1 + _memoryAddress.project.dataProjDir.offset2),
            &tempBuffer,
            sizeof(tempBuffer),
            nullptr
        );
        return string{tempBuffer};
    }
    return {};
}

void InteractionMonitor::insertLineContent(const uint32_t line, const string& content) const {
    const auto functionInsBufLine = StdCallFunction<void(uint32_t, uint32_t, void*)>(
        _baseAddress + _memoryAddress.file.funcInsBufLine.base
    );

    if (const auto fileHandle = _getFileHandle()) {
        CompactString payload(content);
        functionInsBufLine(fileHandle, line, payload.data());
    }
}

void InteractionMonitor::setCaretPosition(const CaretPosition& caretPosition) const {
    const auto SetWndSel = StdCallFunction<void(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)>(
        _baseAddress + _memoryAddress.window.funcSetWndSel.base
    );

    if (const auto windowHandle = _getWindowHandle()) {
        SetWndSel(
            windowHandle,
            caretPosition.line,
            caretPosition.character,
            caretPosition.line,
            caretPosition.character
        );
    }
}

void InteractionMonitor::setLineContent(const uint32_t line, const string& content) const {
    const auto functionPutBufLine = StdCallFunction<void(uint32_t, uint32_t, void*)>(
        _baseAddress + _memoryAddress.file.funcPutBufLine.base
    );

    if (const auto fileHandle = _getFileHandle()) {
        CompactString payload(content);
        functionPutBufLine(fileHandle, line, payload.data());
    }
}

void InteractionMonitor::setSelectedContent(const string& content) const {
    checkValidCodeWindow();

    const auto functionSetBufSelText = StdCallFunction<void(uint32_t, const char*)>(
        _baseAddress + _memoryAddress.window.funcSetBufSelText.base
    );

    uint32_t param1;
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.window.funcSetBufSelText.param1Base),
        &param1,
        sizeof(param1),
        nullptr
    );

    if (param1) {
        functionSetBufSelText(param1, content.c_str());
    }
}

long InteractionMonitor::_keyProcedureHook(const int nCode, const unsigned wParam, const long lParam) {
    if (GetInstance()->_processKeyMessage(wParam, lParam)) {
        return true;
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

long InteractionMonitor::_mouseProcedureHook(const int nCode, const unsigned wParam, const long lParam) {
    GetInstance()->_processMouseMessage(wParam);
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

long InteractionMonitor::_windowProcedureHook(const int nCode, const unsigned int wParam, const long lParam) {
    GetInstance()->_processWindowMessage(lParam);
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

uint32_t InteractionMonitor::_getFileHandle() const {
    checkValidCodeWindow();

    uint32_t fileHandle;
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.file.handle),
        &fileHandle,
        sizeof(fileHandle),
        nullptr
    );
    return fileHandle;
}

uint32_t InteractionMonitor::_getProjectHandle() const {
    checkValidCodeWindow();

    uint32_t handle;
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.project.handle),
        &handle,
        sizeof(handle),
        nullptr
    );
    return handle;
}

uint32_t InteractionMonitor::_getWindowHandle() const {
    uint32_t windowHandle;
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.window.handle),
        &windowHandle,
        sizeof(windowHandle),
        nullptr
    );
    return windowHandle;
}

// ReSharper disable once CppDFAUnreachableFunctionCall
void InteractionMonitor::_handleKeycode(const Keycode keycode) noexcept {
    if (_keyHelper.isPrintable(keycode)) {
        ignore = _handleInteraction(Interaction::NormalInput, _keyHelper.toPrintable(keycode));
        _isSelecting.store(false);
        return;
    }

    const auto keyCombinationOpt = _keyHelper.fromKeycode(keycode);
    try {
        if (keyCombinationOpt.has_value()) {
            if (const auto [key, modifiers] = keyCombinationOpt.value();
                modifiers.empty()) {
                switch (key) {
                    case Key::BackSpace: {
                        ignore = _handleInteraction(Interaction::DeleteInput);
                        _isSelecting.store(false);
                        break;
                    }
                    case Key::Enter: {
                        ignore = _handleInteraction(Interaction::EnterInput);
                        _isSelecting.store(false);
                        break;
                    }
                    case Key::Home:
                    case Key::End:
                    case Key::PageDown:
                    case Key::PageUp:
                    case Key::Left:
                    case Key::Up:
                    case Key::Right:
                    case Key::Down: {
                        _navigateWithKey.store(key);
                        _isSelecting.store(false);
                        // ignore = _handleInteraction(Interaction::SelectionClear);
                        break;
                    }
                    // case Key::F12: {
                    //     //------getSymbolLocationFromLn-----
                    //     //param1: 当前文件句柄
                    //     //param2: 行数
                    //     //param3: 输出参数，自己创建的变量用于后续数据处理，保存的是地址
                    //     //param4: 同3
                    //     //param5: 0
                    //     //description: 获取文件中指定行数的symbol数据，处理后保存到parma3和param4对应的地址中
                    //     const auto getSymbolLocationFromLn = StdCallFunction<int(int, int, int, int, int)>(
                    //         _baseAddress + 0x196A10
                    //     );
                    //
                    //     uint32_t fileHandle;
                    //     ReadProcessMemory(
                    //         _processHandle.get(),
                    //         reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.file.handle),
                    //         &fileHandle,
                    //         sizeof(fileHandle),
                    //         nullptr
                    //     );
                    //     if (fileHandle) {
                    //         //创建变量用于保存数据
                    //         int symbolHandle;
                    //         int symbolSize;
                    //         getSymbolLocationFromLn(fileHandle, 42, (int)&symbolHandle, (int)&symbolSize, 0);
                    //         const auto getSymbolLocationFromLnProceAddress = _baseAddress + 0x19C5E0;
                    //         char content[4096];
                    //         __asm{
                    //                 mov eax, symbolHandle
                    //                 mov edx, symbolSize
                    //                 mov ecx, [eax+14h]
                    //                 mov ecx, [ecx+edx*4]
                    //                 mov edx, [eax+28h]
                    //                 lea ecx, [ecx+edx+4]
                    //                 push ecx
                    //                 push eax
                    //                 lea ecx, content
                    //                 call getSymbolLocationFromLnProceAddress
                    //         }
                    //         // getSymbolLocationFromLnProce(a, b);
                    //         logger::info("11");
                    //         //param1: getSymbolLocationFromLnDate的param3
                    //         //param2: 输出参数，创建的变量
                    //         //description: 将param1参数内容格式化,保存到param2中输出
                    //         const auto getSymbolFormat = StdCallFunction<char*(char*, int)>(
                    //             _baseAddress + 0x1215F0
                    //         );
                    //         CompactString payload;
                    //         if (getSymbolFormat(content, (int)payload.data())) {
                    //             logger::debug("1");
                    //         }
                    //         //---------------SymbolChildren----------------------------
                    //         char y[4096];
                    //         CompactString payload2;
                    //         //param1: int* 这个第一个参数是为了symbol出错后能将出错信息传出"bad Symbol record values"
                    //         //param2: 输出参数，创建的变量用于数据保存
                    //         //param3: SymbolLocation获取到的
                    //         //description: 解析格式化后的symbol数据， 返回值为0表示symbol为空，退出执行
                    //         auto praseSymbolLocation = StdCallFunction<int(int*, int, int)>(
                    //             _baseAddress + 0x127E90
                    //         );
                    //         const auto charlength = strlen(payload.data()->content);
                    //         char str[4096];
                    //         memcpy(str + 2, payload.data()->content, charlength);
                    //         auto result = praseSymbolLocation((int*)payload2.data(), (int)y, (int)str);
                    //         if (!result) {
                    //             break;
                    //         }
                    //         //
                    //         //description: creatSymList
                    //         auto creatSymList = StdCallFunction<int*()>(
                    //             _baseAddress + 0x172B90
                    //         );
                    //         auto symList = creatSymList();
                    //         //
                    //         char z[4096];
                    //         //param1:输出参数, 创建的变量
                    //         //description: 开辟空间，用来保存数据
                    //         auto SymbolProce = StdCallFunction<int*(int*)>(
                    //             _baseAddress + 0x173670
                    //         );
                    //         SymbolProce((int*)z);
                    //
                    //         //param1:praseSymbolLocation的param2
                    //         //param2:creatSymList的返回值,输出参数
                    //         //param3: 0
                    //         //param4: 0,
                    //         //param5:SymbolProce的param1
                    //         //description: 获取到childrenSymbol信息保存到symList中
                    //         auto SymbolChildren = StdCallFunction<int(int, int, int, int, int*)>(
                    //             _baseAddress + 0x175050
                    //         );
                    //         SymbolChildren((int)y, (int)symList, 0, 0, (int*)z);
                    //
                    //         //param1:creatSymList的返回值的地址
                    //         //description: 进一步处理symList内的信息或处理symList
                    //         auto SymListProce = StdCallFunction<int(int)>(
                    //             _baseAddress + 0x12D4E0
                    //         );
                    //         SymListProce((int)&symList);
                    //
                    //         //---------------SymListCount----------------------------
                    //         //symList的首地址保存了它的size
                    //         auto cchild = *symList;
                    //
                    //         //---------------SymListItem----------------------------
                    //
                    //         //param1:symList地址  param2：symList的下标
                    //         //返回值是Item的地址
                    //         //description: 用于获取symList指定索引的地址
                    //         // auto SymbolItem = StdCallFunction<int(int, int)>(
                    //         //     _baseAddress + 0x1F9D
                    //         // );
                    //         //
                    //         // auto item = SymbolItem((int)symList, 10);
                    //         //
                    //         // char v[4096];
                    //         // //param1: (item+12)--是保存的内容起始地址
                    //         // //param2: 输出参数，创建的变量
                    //         // //description: 用于获取Item存储的信息，并保存到param2中输出
                    //         // auto getSymbolItemInfo = StdCallFunction<int(int, int)>(
                    //         //     _baseAddress + 0x810F7
                    //         // );
                    //         //
                    //         // result = getSymbolItemInfo((int)(item + 12), (int)v);
                    //         //
                    //         // CompactString childPayload;
                    //         // getSymbolFormat(v, (int)childPayload.data());
                    //
                    //         //---------------SymbolDeclaredType------------------
                    //
                    //         // char w[4096];
                    //         //
                    //         // const auto childPayloadlength = strlen(childPayload.data()->content);
                    //         // char str1[4096];
                    //         // memcpy(str1 + 2, childPayload.data()->content, childPayloadlength);
                    //         // result = praseSymbolLocation((int*)payload2.data(), (int)w, (int)str1);
                    //         //
                    //         // if (!result) {
                    //         //     break;
                    //         // }
                    //         //param1：praseSymbolLocation的输出参数
                    //         //param2：0
                    //         //param3：1
                    //         //description: 获取自定义变量类型的声明，如果存在则返回1，不存在返回0.
                    //         // auto Declared = StdCallFunction<int(int, int, int)>(
                    //         //     _baseAddress + 0xDA83D
                    //         // );
                    //         // result = Declared((int)w, 0, 1);
                    //         // if (!result) {
                    //         //     break;
                    //         // }
                    //         // CompactString SymbolDeclaredPayload;
                    //         // getSymbolFormat(w, (int)SymbolDeclaredPayload.data());
                    //         // logger::debug(SymbolDeclaredPayload.data()->content);
                    //         //---------------SymListFree------------------
                    //         //param1: creatSymList的返回值
                    //         //description: 将之前申请的内存释放
                    //         // auto symListFree = StdCallFunction<int(int*)>(
                    //         //     _baseAddress + 0xFB25F
                    //         // );
                    //         // symListFree(symList);
                    //     }
                    //     break;
                    // }
                    default: {
                        // TODO: Support Key::Delete
                        break;
                    }
                }
            } else {
                if (modifiers.size() == 1 && modifiers.contains(Modifier::Ctrl)) {
                    switch (key) {
                        case Key::S: {
                            ignore = _handleInteraction(Interaction::Save);
                            break;
                        }
                        case Key::V: {
                            ignore = _handleInteraction(Interaction::Paste);
                            break;
                        }
                        case Key::Z: {
                            ignore = _handleInteraction(Interaction::Undo);
                            break;
                        }
                        default: {
                            break;
                        }
                    }
                }
            }
        }
    } catch (...) {}
}

// ReSharper disable once CppDFAUnreachableFunctionCall
bool InteractionMonitor::_handleInteraction(const Interaction interaction, const any& data) const noexcept {
    bool needBlockMessage{false};
    try {
        for (const auto& handler: _handlerMap.at(interaction)) {
            bool handlerNeedBlockMessage{false};
            handler(data, handlerNeedBlockMessage);
            needBlockMessage |= handlerNeedBlockMessage;
        }
    } catch (out_of_range& e) {
        logger::log(format(
            "No instant handlers for interaction '{}'\n"
            "\tError: {}",
            enum_name(interaction),
            e.what()
        ));
    } catch (exception& e) {
        logger::log(format(
            "Exception when processing instant interaction '{}' : {}",
            enum_name(interaction),
            e.what()
        ));
    }
    return needBlockMessage;
}

void InteractionMonitor::_monitorCaretPosition() {
    thread([this] {
        while (_isRunning.load()) {
            if (const auto navigationBufferOpt = _navigateWithKey.load();
                navigationBufferOpt.has_value()) {
                ignore = _handleInteraction(Interaction::NavigateWithKey, navigationBufferOpt.value());
                _navigateWithKey.store(nullopt);
                continue;
            }

            auto newCursorPosition = getCaretPosition();
            newCursorPosition.maxCharacter = newCursorPosition.character;
            if (const auto oldCursorPosition = _currentCaretPosition.load();
                oldCursorPosition != newCursorPosition) {
                _currentCaretPosition.store(newCursorPosition);
                if (const auto navigateWithMouseOpt = _navigateWithMouse.load();
                    navigateWithMouseOpt.has_value()) {
                    _handleInteraction(Interaction::NavigateWithMouse,
                                       make_tuple(newCursorPosition, oldCursorPosition));
                    _navigateWithMouse.store(nullopt);
                }
            }
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }).detach();
}

// ReSharper disable once CppDFAUnreachableFunctionCall
Range InteractionMonitor::_monitorCursorSelect() const {
    Range select{};
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.window.dataSelection.lineStart.base),
        &select.start.line,
        sizeof(select.start.line),
        nullptr
    );
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.window.dataSelection.characterStart.base),
        &select.start.character,
        sizeof(select.start.character),
        nullptr
    );
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.window.dataSelection.lineEnd.base),
        &select.end.line,
        sizeof(select.end.line),
        nullptr
    );
    ReadProcessMemory(
        _processHandle.get(),
        reinterpret_cast<LPCVOID>(_baseAddress + _memoryAddress.window.dataSelection.characterEnd.base),
        &select.end.character,
        sizeof(select.end.character),
        nullptr
    );
    return select;
}

void InteractionMonitor::_monitorDebugLog() const {
    thread([this] {
        while (_isRunning.load()) {
            if (const auto debugStringOpt = system::getEnvironmentVariable(debugLogKey);
                debugStringOpt.has_value()) {
                logger::debug(format("[SI] {}", regex_replace(debugStringOpt.value(), regex("\\n"), "\n")));
                system::setEnvironmentVariable(debugLogKey);
            }
            this_thread::sleep_for(chrono::milliseconds(5));
        }
    }).detach();
}

// ReSharper disable once CppDFAUnreachableFunctionCall
bool InteractionMonitor::_processKeyMessage(const unsigned wParam, const unsigned lParam) {
    if (!WindowManager::GetInstance()->hasValidCodeWindow()) {
        return false;
    }

    const auto keyFlags = HIWORD(lParam);
    const auto isKeyUp = (keyFlags & KF_UP) == KF_UP;
    bool needBlockMessage{false};

    switch (wParam) {
        case VK_RETURN: {
            if (isKeyUp) {
                needBlockMessage = true;
            } else if (WindowManager::GetInstance()->hasPopListWindow()) {
                ignore = _handleInteraction(Interaction::CompletionCancel, true);
            }
            break;
        }
        case VK_ESCAPE: {
            if (isKeyUp) {
                needBlockMessage = true;
            } else if (!WindowManager::GetInstance()->hasPopListWindow()) {
                ignore = _handleInteraction(Interaction::CompletionCancel, false);
            }
            _isSelecting.store(false);
            break;
        }
        case VK_TAB: {
            if (isKeyUp) {
                needBlockMessage = true;
            } else if (WindowManager::GetInstance()->hasPopListWindow()) {
                ignore = _handleInteraction(Interaction::CompletionCancel, true);
            } else {
                needBlockMessage = _handleInteraction(Interaction::CompletionAccept);
            }
            break;
        }
        default: {
            break;
        }
    }
    return needBlockMessage;
}

// ReSharper disable once CppDFAUnreachableFunctionCall
void InteractionMonitor::_processMouseMessage(const unsigned wParam) {
    switch (wParam) {
        case WM_LBUTTONDOWN: {
            if (!_isMouseLeftDown.load()) {
                _isMouseLeftDown.store(true);
            }
            _navigateWithMouse.store(Mouse::Left);
            break;
        }
        case WM_MOUSEMOVE: {
            if (_isMouseLeftDown.load()) {
                _isSelecting.store(true);
            }
            break;
        }
        case WM_LBUTTONUP: {
            // if (_isSelecting.load()) {
            //     auto selectRange = _monitorCursorSelect();
            //     ignore = _handleInteraction(Interaction::SelectionSet, selectRange);
            // } else {
            //     ignore = _handleInteraction(Interaction::SelectionClear);
            // }
            _isMouseLeftDown.store(false);
            break;
        }
        case WM_LBUTTONDBLCLK: {
            _isSelecting.store(true);
            break;
        }
        default: {
            break;
        }
    }
}

// ReSharper disable once CppDFAUnreachableFunctionCall
void InteractionMonitor::_processWindowMessage(const long lParam) {
    const auto windowProcData = reinterpret_cast<PCWPSTRUCT>(lParam);
    if (const auto currentWindow = reinterpret_cast<int64_t>(windowProcData->hwnd);
        window::getWindowClassName(currentWindow) == "si_Sw") {
        switch (windowProcData->message) {
            case WM_KILLFOCUS: {
                if (WindowManager::GetInstance()->checkNeedHideWhenLostFocus(windowProcData->wParam)) {
                    WebsocketManager::GetInstance()->send(EditorFocusStateClientMessage(false));
                }
                break;
            }
            case WM_SETFOCUS: {
                if (WindowManager::GetInstance()->checkNeedShowWhenGainFocus(currentWindow)) {
                    WebsocketManager::GetInstance()->send(EditorFocusStateClientMessage(true));
                }
                break;
            }
            case UM_KEYCODE: {
                _handleKeycode(windowProcData->wParam);
                break;
            }
            default: {
                break;
            }
        }
    }
}
