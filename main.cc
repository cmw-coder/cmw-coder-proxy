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

            InteractionMonitor::GetInstance()->addHandler(
                Interaction::AcceptCompletion,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionAccept
            );
            InteractionMonitor::GetInstance()->addHandler(
                Interaction::CancelCompletion,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionCancel
            );
            InteractionMonitor::GetInstance()->addHandler(
                Interaction::DeleteInput,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionDelete
            );
            InteractionMonitor::GetInstance()->addHandler(
                Interaction::EnterInput,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionEnter
            );
            InteractionMonitor::GetInstance()->addHandler(
                Interaction::Navigate,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionNavigate
            );
            InteractionMonitor::GetInstance()->addHandler(
                Interaction::NormalInput,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionNormal
            );
            InteractionMonitor::GetInstance()->addHandler(
                Interaction::Paste,
                WindowManager::GetInstance(),
                &WindowManager::interactionPaste
            );
            InteractionMonitor::GetInstance()->addHandler(
                Interaction::Save,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionSave
            );
            InteractionMonitor::GetInstance()->addHandler(
                Interaction::Undo,
                CompletionManager::GetInstance(),
                &CompletionManager::interactionUndo
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
