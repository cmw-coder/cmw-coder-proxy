#pragma once

namespace types {
    enum class WsAction {
        CompletionAccept,
        CompletionCache,
        CompletionCancel,
        CompletionEdit,
        CompletionGenerate,
        CompletionSelect,
        DebugSync,
        EditorFocusState,
        EditorSwitchProject,
        HandShake
    };
}
