#include <format>

#include <components/CompletionManager.h>
#include <components/ConfigManager.h>
#include <components/InteractionMonitor.h>
#include <components/MemoryManipulator.h>
#include <components/ModificationManager.h>
#include <components/ModuleProxy.h>
#include <components/SymbolManager.h>
#include <components/WindowManager.h>
#include <components/WebsocketManager.h>
#include <utils/iconv.h>
#include <utils/logger.h>
#include <utils/system.h>

#include <windows.h>

using namespace components;
using namespace models;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    void initialize() {
        logger::info("Comware Coder Proxy is initializing...");

        ModuleProxy::Construct();
        ConfigManager::Construct();
        MemoryManipulator::Construct(ConfigManager::GetInstance()->version());
        WebsocketManager::Construct("ws://127.0.0.1:3000");
        ModificationManager::Construct();
        SymbolManager::Construct();
        CompletionManager::Construct();
        InteractionMonitor::Construct();
        WindowManager::Construct();
    }

    void finalize() {
        logger::info("Comware Coder Proxy is finalizing...");

        WindowManager::Destruct();
        InteractionMonitor::Destruct();
        CompletionManager::Destruct();
        SymbolManager::Destruct();
        ModificationManager::Destruct();
        WebsocketManager::Destruct();
        MemoryManipulator::Destruct();
        ConfigManager::Destruct();
        ModuleProxy::Destruct();
    }
}

#ifdef __cplusplus
extern "C" {
#endif

BOOL __stdcall DllMain(const HMODULE hModule, const DWORD dwReason, [[maybe_unused]] PVOID pvReserved) {
    switch (dwReason) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hModule);

            initialize();

            InteractionMonitor::GetInstance()->registerInteraction(
                Interaction::CompletionAccept,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionCompletionAccept
            );
            InteractionMonitor::GetInstance()->registerInteraction(
                Interaction::CompletionCancel,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionCompletionCancel
            );
            InteractionMonitor::GetInstance()->registerInteraction(
                Interaction::DeleteInput,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionDeleteInput
            );
            InteractionMonitor::GetInstance()->registerInteraction(
                Interaction::EnterInput,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionEnterInput
            );
            InteractionMonitor::GetInstance()->registerInteraction(
                Interaction::NavigateWithKey,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionNavigateWithKey
            );
            InteractionMonitor::GetInstance()->registerInteraction(
                Interaction::NavigateWithMouse,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionNavigateWithMouse
            );
            InteractionMonitor::GetInstance()->registerInteraction(
                Interaction::NormalInput,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionNormalInput
            );
            InteractionMonitor::GetInstance()->registerInteraction(
                Interaction::Paste,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionPaste
            );
            InteractionMonitor::GetInstance()->registerInteraction(
                Interaction::Save,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionSave
            );
            InteractionMonitor::GetInstance()->registerInteraction(
                Interaction::Undo,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionUndo
            );

            WebsocketManager::GetInstance()->registerAction(
                WsAction::ChatInsert,
                [](nlohmann::json&& data) {
                    if (const auto serverMessage = ChatInsertServerMessage(move(data));
                        serverMessage.result == "success") {
                        auto content = serverMessage.content().value();
                        if (content = iconv::autoEncode(content);
                            !content.empty()) {
                            while (!WindowManager::GetInstance()->getCurrentWindowHandle().has_value()) {
                                this_thread::sleep_for(5ms);
                            }

                            const auto memoryManipulator = MemoryManipulator::GetInstance();
                            const auto currentPosition = memoryManipulator->getCaretPosition();

                            uint32_t insertedlineCount{0}, lastLineLength{0};
                            for (const auto lineRange: content | views::split("\n"sv)) {
                                auto lineContent = string{lineRange.begin(), lineRange.end()};
                                if (insertedlineCount == 0) {
                                    lastLineLength = currentPosition.character + 1 + lineContent.size();
                                    memoryManipulator->setSelectionContent(lineContent);
                                } else {
                                    lastLineLength = lineContent.size();
                                    memoryManipulator->setLineContent(currentPosition.line + insertedlineCount,
                                                                      lineContent, true);
                                }
                                ++insertedlineCount;
                            }
                            WindowManager::GetInstance()->sendLeftThenRight();
                            memoryManipulator->setCaretPosition({
                                lastLineLength, currentPosition.line + insertedlineCount - 1
                            });
                        }
                    }
                }
            );
            WebsocketManager::GetInstance()->registerAction(
                WsAction::CompletionGenerate,
                CompletionManager::GetInstance(),
                &CompletionManager::wsCompletionGenerate
            );
            WebsocketManager::GetInstance()->registerAction(
                WsAction::SettingSync,
                ConfigManager::GetInstance(),
                &ConfigManager::wsSettingSync
            );

            logger::info(format(
                R"([{}] Comware Coder Proxy {} is ready. CurrentTID: {}, mainTID: {}, mainModuleName: "{}")",
                GetCurrentProcessId(),
                VERSION_STRING,
                GetCurrentThreadId(),
                system::getMainThreadId(),
                system::getModuleFileName(reinterpret_cast<uint64_t>(GetModuleHandle(nullptr)))
            ));
            break;
        }
        case DLL_PROCESS_DETACH: {
            finalize();
            logger::info("Comware Coder Proxy is unloaded");
            break;
        }
        default: {
            break;
        }
    }
    return TRUE;
}

#ifdef __cplusplus
}
#endif
