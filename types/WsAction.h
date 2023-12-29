#pragma once

namespace types {
    enum class WsAction {
        CompletionAccept,
        CompletionCache,
        CompletionCancel,
        CompletionGenerate,
        DebugSync,
    };
}
