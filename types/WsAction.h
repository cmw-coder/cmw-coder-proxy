#pragma once

namespace types {
    enum class WsAction {
        ChatInsert,
        CompletionAccept,
        CompletionCache,
        CompletionCancel,
        CompletionEdit,
        CompletionGenerate,
        CompletionSelect,
        DebugSync,
        EditorFocusState,
        EditorPaste,
        EditorSwitchProject,
        HandShake,
        SettingSync,
    };
}
