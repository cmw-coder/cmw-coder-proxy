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
        EditorConfig,
        EditorPaste,
        EditorSelection,
        EditorState,
        EditorSwitchFile,
        EditorSwitchProject,
        HandShake,
        ReviewRequest,
    };
}
