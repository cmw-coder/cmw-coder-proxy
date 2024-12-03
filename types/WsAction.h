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
        EditorPaste,
        EditorSelection,
        EditorState,
        EditorSwitchFile,
        EditorSwitchProject,
        HandShake,
        ReviewRequest,
        SettingSync,
    };
}
