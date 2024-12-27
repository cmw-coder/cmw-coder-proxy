#include <format>

#include <components/CompletionManager.h>
#include <components/ConfigManager.h>
#include <components/InteractionMonitor.h>
#include <components/MemoryManipulator.h>
#include <components/ModuleProxy.h>
#include <components/StatisticManager.h>
#include <components/SymbolManager.h>
#include <components/WindowManager.h>
#include <components/WebsocketManager.h>
#include <models/WsMessage.h>
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
        WindowManager::Construct();
        WebsocketManager::Construct("ws://127.0.0.1:3000");
        SymbolManager::Construct();
        InteractionMonitor::Construct();
        StatisticManager::Construct();
        CompletionManager::Construct();
    }

    void finalize() {
        logger::info("Comware Coder Proxy is finalizing...");

        CompletionManager::Destruct();
        StatisticManager::Construct();
        InteractionMonitor::Destruct();
        SymbolManager::Destruct();
        WebsocketManager::Destruct();
        WindowManager::Destruct();
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
                Interaction::SelectionReplace,
                [](const std::any& data, bool&) {
                    try {
                        if (const auto [startLine, count] = any_cast<pair<uint32_t, int32_t>>(data);
                            count > 0) {
                            StatisticManager::GetInstance()->addLine(startLine, count);
                        } else if (count < 0) {
                            StatisticManager::GetInstance()->removeLine(startLine, -count);
                        }
                    } catch (const bad_any_cast& e) {
                        logger::log(format("Invalid interactionSelectionReplace data: {}", e.what()));
                    }
                }
            );
            InteractionMonitor::GetInstance()->registerInteraction(
                Interaction::Undo,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionUndo
            );
            // WebsocketManager::GetInstance()->registerAction(
            //     WsAction::ChatInsert,
            //     [](nlohmann::json&& data) {
            //         if (const auto serverMessage = ChatInsertServerMessage(move(data));
            //             serverMessage.result == "success") {
            //             auto content = serverMessage.content().value();
            //             if (content = iconv::autoEncode(content);
            //                 !content.empty()) {
            //                 while (!WindowManager::GetInstance()->getCurrentWindowHandle().has_value()) {
            //                     this_thread::sleep_for(5ms);
            //                 }
            //                 common::insertContent(content);
            //             }
            //         }
            //     }
            // );
            WebsocketManager::GetInstance()->registerAction(
                WsAction::CompletionGenerate,
                CompletionManager::GetInstance(),
                &CompletionManager::wsCompletionGenerate
            );
            WebsocketManager::GetInstance()->registerAction(
                WsAction::EditorConfig,
                [](nlohmann::json&& data) {
                    if (const auto serverMessage = EditorConfigServerMessage(move(data));
                        serverMessage.result == "success") {
                        if (const auto completionConfigOpt = serverMessage.completionConfig();
                            completionConfigOpt.has_value()) {
                            CompletionManager::GetInstance()->updateCompletionConfig(completionConfigOpt.value());
                        }
                        if (const auto genericConfigOpt = serverMessage.genericConfig();
                            genericConfigOpt.has_value()) {
                            InteractionMonitor::GetInstance()->updateGenericConfig(genericConfigOpt.value());
                        }
                        if (const auto shortcutConfigOpt = serverMessage.shortcutConfig();
                            shortcutConfigOpt.has_value()) {
                            InteractionMonitor::GetInstance()->updateShortcutConfig(shortcutConfigOpt.value());
                        }
                        if (const auto statisticConfigOpt = serverMessage.statisticConfig();
                            statisticConfigOpt.has_value()) {
                            StatisticManager::GetInstance()->updateStatisticConfig(statisticConfigOpt.value());
                        }
                    }
                }
            );
            WebsocketManager::GetInstance()->registerAction(
                WsAction::ReviewRequest,
                [](nlohmann::json&& data) {
                    if (const auto serverMessage = ReviewRequestServerMessage(move(data));
                        serverMessage.result == "success") {
                        const auto reviewReferences =
                                SymbolManager::GetInstance()->getReviewReferences(
                                    serverMessage.content(), serverMessage.path(), 0
                                )
                                | views::values
                                | views::filter([&serverMessage](const ReviewReference& reviewReference) {
                                    try {
                                        return reviewReference.path != serverMessage.path() ||
                                               reviewReference.startLine > serverMessage.selection().end.line ||
                                               serverMessage.selection().begin.line > reviewReference.endLine;
                                    } catch (exception& e) {
                                        logger::warn(format("Error checking '{}': {}", reviewReference.name, e.what()));
                                    }
                                    return true;
                                })
                                | ranges::to<vector<ReviewReference>>();
                        logger::debug(format("reviewReferenceMap count: {}", reviewReferences.size()));
                        WebsocketManager::GetInstance()->send(ReviewRequestClientMessage{
                            serverMessage.id(),
                            reviewReferences
                        });
                    }
                }
            );

            logger::info(format(
                R"([{}] Comware Coder Proxy {} is ready. CurrentTID: {}, mainTID: {}, mainModuleName: "{}")",
                GetCurrentProcessId(),
                VERSION_STRING,
                GetCurrentThreadId(),
                system::getMainThreadId(GetCurrentProcessId()),
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
