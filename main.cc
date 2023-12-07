#include <format>

#include <components/CompletionManager.h>
#include <components/Configurator.h>
#include <components/InteractionMonitor.h>
#include <components/ModificationManager.h>
#include <components/ModuleProxy.h>
#include <components/WindowManager.h>
#include <utils/logger.h>
#include <utils/system.h>

#include <windows.h>

using namespace components;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    void initialize() {
        logger::log("Comware Coder Proxy is initializing...");

        ModuleProxy::Construct();
        Configurator::Construct();
        ModificationManager::Construct();
        CompletionManager::Construct();
        InteractionMonitor::Construct();
        WindowManager::Construct();

        ModificationManager::GetInstance()->addTab(
            "main.c",
            "C:/Users/particleg/Documents/Source Insight 4.0/Projects/FunctionTest/main.c"
        );
    }

    void finalize() {
        logger::log("Comware Coder Proxy is finalizing...");

        WindowManager::Destruct();
        InteractionMonitor::Destruct();
        CompletionManager::Destruct();
        ModificationManager::Destruct();
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

            InteractionMonitor::GetInstance()->addDelayedHandler(
                Interaction::DeleteInput,
                CompletionManager::GetInstance(),
                &CompletionManager::delayedDelete
            );
            InteractionMonitor::GetInstance()->addDelayedHandler(
                Interaction::DeleteInput,
                ModificationManager::GetInstance(),
                &ModificationManager::delayedDelete
            );
            InteractionMonitor::GetInstance()->addDelayedHandler(
                Interaction::EnterInput,
                CompletionManager::GetInstance(),
                &CompletionManager::delayedEnter
            );
            InteractionMonitor::GetInstance()->addDelayedHandler(
                Interaction::EnterInput,
                ModificationManager::GetInstance(),
                &ModificationManager::delayedEnter
            );
            InteractionMonitor::GetInstance()->addDelayedHandler(
                Interaction::Navigate,
                CompletionManager::GetInstance(),
                &CompletionManager::delayedNavigate
            );
            InteractionMonitor::GetInstance()->addDelayedHandler(
                Interaction::NormalInput,
                ModificationManager::GetInstance(),
                &ModificationManager::delayedNormal
            );

            InteractionMonitor::GetInstance()->addInstantHandler(
                Interaction::AcceptCompletion,
                CompletionManager::GetInstance(),
                &CompletionManager::instantAccept
            );
            InteractionMonitor::GetInstance()->addInstantHandler(
                Interaction::CancelCompletion,
                CompletionManager::GetInstance(),
                &CompletionManager::instantCancel
            );
            InteractionMonitor::GetInstance()->addInstantHandler(
                Interaction::Navigate,
                CompletionManager::GetInstance(),
                &CompletionManager::instantNavigate
            );
            InteractionMonitor::GetInstance()->addInstantHandler(
                Interaction::NormalInput,
                CompletionManager::GetInstance(),
                &CompletionManager::instantNormal
            );
            InteractionMonitor::GetInstance()->addInstantHandler(
                Interaction::Paste,
                WindowManager::GetInstance(),
                &WindowManager::interactionPaste
            );
            InteractionMonitor::GetInstance()->addInstantHandler(
                Interaction::Save,
                CompletionManager::GetInstance(),
                &CompletionManager::instantSave
            );
            InteractionMonitor::GetInstance()->addInstantHandler(
                Interaction::Undo,
                CompletionManager::GetInstance(),
                &CompletionManager::instantUndo
            );

            const auto mainThreadId = system::getMainThreadId();
            logger::log(std::format(
                "siVersion: {}, PID: {}, currentTID: {}, mainTID: {}, mainModuleName: {}",
                Configurator::GetInstance()->reportVersion(""),
                GetCurrentProcessId(),
                GetCurrentThreadId(),
                mainThreadId,
                system::getModuleFileName(reinterpret_cast<uint64_t>(GetModuleHandle(nullptr)))
            ));

            logger::log("Comware Coder Proxy is ready");
            break;
        }
        case DLL_PROCESS_DETACH: {
            finalize();
            logger::log("Comware Coder Proxy is unloaded");
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
