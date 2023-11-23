#include <components/WindowManager.h>
#include <types/CompletionManager.h>

using namespace types;

void CompletionManager::acceptCompletion() {
    _isJustInserted = true;
    if (auto oldCompletion = _completionCache.reset(); !oldCompletion.content().empty()) {
        components::WindowManager::GetInstance()->sendAcceptCompletion();
        logger::log(format("Accepted completion: {}", oldCompletion.stringify()));
        thread(&RegistryMonitor::_reactToCompletion, this, move(oldCompletion), true).detach();
    }
    else {
        _retrieveEditorInfo();
    }
}
