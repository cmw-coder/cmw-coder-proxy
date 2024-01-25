#pragma once

namespace types {
    enum class WsAction {
        CompletionAccept,
        CompletionCache,
        CompletionCancel,
        CompletionGenerate,
        CompletionSelect,
        DebugSync,
        EditorFocusState,
        EditorSwitchProject,
        HandShake
    };
}
