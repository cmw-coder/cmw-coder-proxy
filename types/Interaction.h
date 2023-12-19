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
        Undo,
        Select,
        ClearSelect,
    };
}
