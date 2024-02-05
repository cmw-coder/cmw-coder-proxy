#include <format>
#include <regex>

#include <magic_enum.hpp>

#include <components/CompletionManager.h>
#include <components/Configurator.h>
#include <components/InteractionMonitor.h>
#include <components/MemoryManipulator.h>
#include <components/WebsocketManager.h>
#include <components/WindowManager.h>
#include <models/MemoryPayloads.h>
#include <types/AddressToFunction.h>
#include <utils/logger.h>
#include <utils/memory.h>
#include <utils/system.h>
#include <utils/window.h>

#include <windows.h>

using namespace components;
using namespace magic_enum;
using namespace models;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    constexpr auto debugLogKey = "CMWCODER_debugLog";
}

InteractionMonitor::InteractionMonitor()
    : _keyHelper(Configurator::GetInstance()->version().first),
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

    _monitorCaretPosition();
    _monitorDebugLog();

    logger::info("InteractionMonitor is initialized.");
}

InteractionMonitor::~InteractionMonitor() {
    _isRunning.store(false);
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
                    case Key::F12: {
                        const auto memoryManipulator = MemoryManipulator::GetInstance();
                        if (const auto symbolNameOpt = memoryManipulator->getSymbolName();
                            symbolNameOpt.has_value()) {
                            const auto symbolName = symbolNameOpt.value();
                            /**
                             * ---------------SymbolChildren----------------------------
                             */
                            // SymbolName childrenSymbolName;
                            // SimpleString errorMessage;
                            // //param1: int* 这个第一个参数是为了symbol出错后能将出错信息传出"bad Symbol record values"
                            // //param2: 输出参数，创建的变量用于数据保存
                            // //param3: SymbolLocation获取到的
                            // //description: 解析格式化后的symbol数据， 返回值为0表示symbol为空，退出执行
                            // auto getSymbolNameFromSymbolRecord = AddressToFunction<bool(void*, void*, const void*)>(
                            //     _baseAddress + 0xC62ED
                            // );
                            //
                            // if (!getSymbolNameFromSymbolRecord(
                            //     errorMessage.data(),
                            //     childrenSymbolName.data(),
                            //     symbolRecord.data()
                            // )) {
                            //     break;
                            // }

                            const auto creatSymList = AddressToFunction<SymbolListHandle()>(
                                memory::offset(0xFB214)
                            );
                            auto symList = creatSymList();

                            SymbolBuffer symbolBuffer;
                            const auto bufferInitializer = AddressToFunction<void(void*)>(
                                memory::offset(0xFB90B)
                            );
                            bufferInitializer(symbolBuffer.data());

                            const auto SymbolChildren = AddressToFunction<
                                int(const void*, SymbolListHandle, int, int, void*)
                            >(memory::offset(0xFD8CF));
                            const auto a = SymbolChildren(symbolName.data(), symList, 0, 0, symbolBuffer.data());

                            //param1:creatSymList的返回值的地址
                            //description: 进一步处理symList内的信息或处理symList
                            auto SymListProce = AddressToFunction<int(SymbolListHandle*)>(
                                memory::offset(0xD26D5)
                            );
                            const auto b = SymListProce(&symList);

                            /**
                             * ---------------SymListCount----------------------------
                             */
                            // //symList的首地址保存了它的size
                            // auto cchild = *symList;
                            //
                            // //---------------SymListItem----------------------------
                            //
                            // //param1:symList地址  param2：symList的下标
                            // //返回值是Item的地址
                            // //description: 用于获取symList指定索引的地址
                            // auto SymbolItem = AddressToFunction<int(int, int)>(
                            //     _baseAddress + 0x1F9D
                            // );
                            //
                            // auto item = SymbolItem((int) symList, 10);
                            //
                            // char v[4096];
                            // //param1: (item+12)--是保存的内容起始地址
                            // //param2: 输出参数，创建的变量
                            // //description: 用于获取Item存储的信息，并保存到param2中输出
                            // auto getSymbolItemInfo = AddressToFunction<int(int, int)>(
                            //     _baseAddress + 0x810F7
                            // );
                            // //
                            // result = getSymbolItemInfo((int) (item + 12), (int) v);
                            //
                            // SimpleString childPayload;
                            // getSymbolRecord(v, childPayload.data());
                            //
                            // //---------------SymbolDeclaredType------------------
                            //
                            // char w[4096];
                            //
                            // const auto childPayloadlength = strlen(childPayload.data()->content);
                            // char str1[4096];
                            // memcpy(str1 + 2, childPayload.data()->content, childPayloadlength);
                            // result = praseSymbolLocation((int *) payload2.data(), (int) w, (int) str1);
                            //
                            // if (!result) {
                            //     break;
                            // }
                            // //param1：praseSymbolLocation的输出参数
                            // //param2：0
                            // //param3：1
                            // //description: 获取自定义变量类型的声明，如果存在则返回1，不存在返回0.
                            // auto Declared = AddressToFunction<int(int, int, int)>(
                            //     _baseAddress + 0xDA83D
                            // );
                            // result = Declared((int) w, 0, 1);
                            // if (!result) {
                            //     break;
                            // }
                            // SimpleString SymbolDeclaredPayload;
                            // getSymbolRecord(w, SymbolDeclaredPayload.data());
                            // logger::debug(SymbolDeclaredPayload.data()->content);


                            auto symListFree = AddressToFunction<int(SymbolListHandle)>(
                                memory::offset(0xFB25F)
                            );
                            symListFree(symList);
                            logger::debug("1");
                        }
                        break;
                    }
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

            auto newCursorPosition = MemoryManipulator::GetInstance()->getCaretPosition();
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
