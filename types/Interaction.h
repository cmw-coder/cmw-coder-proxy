#pragma once

namespace types {
    enum class Interaction {
        CompletionAccept,
        CompletionCancel,
        DeleteInput,
        EnterInput,
        NavigateWithKey,
        NavigateWithMouse,
        NormalInput,
        Paste,
        Save,
        SelectionSet,
        SelectionClear,
        Undo,
    };
}
