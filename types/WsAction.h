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
        EditorCommit,
        EditorFocusState,
        EditorPaste,
        EditorSwitchProject,
        EditorSwitchSvn,
        HandShake,
        SettingSync,
    };
}
