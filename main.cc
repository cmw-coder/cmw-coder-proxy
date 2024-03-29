#include <format>

#include <components/CompletionManager.h>
#include <components/Configurator.h>
#include <components/InteractionMonitor.h>
#include <components/MemoryManipulator.h>
#include <components/ModificationManager.h>
#include <components/ModuleProxy.h>
#include <components/WindowManager.h>
#include <components/WebsocketManager.h>
#include <utils/logger.h>
#include <utils/system.h>

#include <windows.h>

using namespace components;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    void initialize() {
        logger::info("Comware Coder Proxy is initializing...");

        ModuleProxy::Construct();
        Configurator::Construct();
        MemoryManipulator::Construct(Configurator::GetInstance()->version());
        WebsocketManager::Construct("ws://127.0.0.1:3000");
        ModificationManager::Construct();
        CompletionManager::Construct();
        InteractionMonitor::Construct();
        WindowManager::Construct();
    }

    void finalize() {
        logger::info("Comware Coder Proxy is finalizing...");

        WindowManager::Destruct();
        InteractionMonitor::Destruct();
        CompletionManager::Destruct();
        ModificationManager::Destruct();
        WebsocketManager::Destruct();
        MemoryManipulator::Destruct();
        Configurator::Destruct();
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
            // InteractionMonitor::GetInstance()->registerInteraction(
            //     Interaction::AcceptCompletion,
            //     ModificationManager::GetInstance(),
            //     &ModificationManager::interactionAcceptCompletion
            // );
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
            // InteractionMonitor::GetInstance()->registerInteraction(
            //     Interaction::DeleteInput,
            //     ModificationManager::GetInstance(),
            //     &ModificationManager::interactionDeleteInput
            // );
            InteractionMonitor::GetInstance()->registerInteraction(
                Interaction::EnterInput,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionEnterInput
            );
            // InteractionMonitor::GetInstance()->registerInteraction(
            //     Interaction::EnterInput,
            //     ModificationManager::GetInstance(),
            //     &ModificationManager::interactionEnterInput
            // );
            InteractionMonitor::GetInstance()->registerInteraction(
                Interaction::NavigateWithKey,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionNavigateWithKey
            );
            // InteractionMonitor::GetInstance()->registerInteraction(
            //     Interaction::NavigateWithKey,
            //     ModificationManager::GetInstance(),
            //     &ModificationManager::interactionNavigateWithKey
            // );
            InteractionMonitor::GetInstance()->registerInteraction(
                Interaction::NavigateWithMouse,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionNavigateWithMouse
            );
            // InteractionMonitor::GetInstance()->registerInteraction(
            //     Interaction::NavigateWithMouse,
            //     ModificationManager::GetInstance(),
            //     &ModificationManager::interactionNavigateWithMouse
            // );
            InteractionMonitor::GetInstance()->registerInteraction(
                Interaction::NormalInput,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionNormalInput
            );
            // InteractionMonitor::GetInstance()->registerInteraction(
            //     Interaction::NormalInput,
            //     ModificationManager::GetInstance(),
            //     &ModificationManager::interactionNormalInput
            // );
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
            // InteractionMonitor::GetInstance()->registerInteraction(
            //     Interaction::Save,
            //     ModificationManager::GetInstance(),
            //     &ModificationManager::interactionSave
            // );
            // InteractionMonitor::GetInstance()->registerInteraction(
            //     Interaction::SelectionSet,
            //     ModificationManager::GetInstance(),
            //     &ModificationManager::interactionSelectionSet
            // );
            // InteractionMonitor::GetInstance()->registerInteraction(
            //     Interaction::SelectionClear,
            //     ModificationManager::GetInstance(),
            //     &ModificationManager::interactionSelectionClear
            // );
            InteractionMonitor::GetInstance()->registerInteraction(
                Interaction::Undo,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionUndo
            );

            WebsocketManager::GetInstance()->registerAction(
                WsAction::CompletionGenerate,
                CompletionManager::GetInstance(),
                &CompletionManager::wsActionCompletionGenerate
            );

            logger::info(format(
                "Version: {}, PID: {}, currentTID: {}, mainTID: {}, mainModuleName: {}",
                VERSION_STRING + Configurator::GetInstance()->reportVersion(),
                GetCurrentProcessId(),
                GetCurrentThreadId(),
                system::getMainThreadId(),
                system::getModuleFileName(reinterpret_cast<uint64_t>(GetModuleHandle(nullptr)))
            ));

            logger::info("Comware Coder Proxy is ready");
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
