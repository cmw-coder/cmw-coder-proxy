#include <format>

#include <ixwebsocket/IXNetSystem.h>

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

        ix::initNetSystem();

        ModuleProxy::Construct();
        Configurator::Construct();
        ModificationManager::Construct();
        CompletionManager::Construct();
        InteractionMonitor::Construct();
        WindowManager::Construct();

        ModificationManager::GetInstance()->addTab(
            "main.c",
            "C:/Users/particleg/Documents/Source Insight/Projects/FunctionTest/main.c"
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

        ix::uninitNetSystem();
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

            InteractionMonitor::GetInstance()->addInstantHandler(
                Interaction::CaretUpdate,
                ModificationManager::GetInstance(),
                &ModificationManager::instantCaret
            );
            InteractionMonitor::GetInstance()->addInstantHandler(
                Interaction::DeleteInput,
                ModificationManager::GetInstance(),
                &ModificationManager::instantDelete
            );
            InteractionMonitor::GetInstance()->addInstantHandler(
                Interaction::EnterInput,
                ModificationManager::GetInstance(),
                &ModificationManager::instantEnter
            );
            InteractionMonitor::GetInstance()->addInstantHandler(
                Interaction::Navigate,
                ModificationManager::GetInstance(),
                &ModificationManager::instantNavigate
            );
            InteractionMonitor::GetInstance()->addInstantHandler(
                Interaction::NormalInput,
                ModificationManager::GetInstance(),
                &ModificationManager::instantNormal
            );
            InteractionMonitor::GetInstance()->addInstantHandler(
                Interaction::SelectionSet,
                ModificationManager::GetInstance(),
                &ModificationManager::instantSelect
            );
            InteractionMonitor::GetInstance()->addInstantHandler(
                Interaction::SelectionClear,
                ModificationManager::GetInstance(),
                &ModificationManager::instantClearSelect
            );
            // InteractionMonitor::GetInstance()->addInstantHandler(
            //     Interaction::AcceptCompletion,
            //     CompletionManager::GetInstance(),
            //     &CompletionManager::instantAccept
            // );

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
