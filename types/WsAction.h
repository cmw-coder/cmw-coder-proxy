#pragma once

namespace types {
    enum class WsAction {
        CompletionAccept,
        CompletionCache,
        CompletionCancel,
        CompletionGenerate,
        CompletionKept,
        CompletionSelect,
        DebugSync,
        EditorFocusState,
        EditorSwitchProject,
        HandShake
    };
}
