#pragma once

namespace types {
    enum class Interaction {
        AcceptCompletion,
        CancelCompletion,
        CaretUpdate,
        DeleteInput,
        EnterInput,
        Navigate,
        NormalInput,
        Paste,
        Save,
        SelectionSet,
        SelectionClear,
        Undo,
    };
}
