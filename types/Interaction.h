#pragma once

namespace types {
    enum class Interaction {
        AcceptCompletion,
        CancelCompletion,
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
