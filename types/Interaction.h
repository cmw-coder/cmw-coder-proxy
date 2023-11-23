#pragma once

namespace types {
    enum class Interaction {
        AcceptCompletion,
        CancelCompletion,
        DeleteInput,
        EnterInput,
        LostFocus,
        MouseClick,
        Navigate,
        NormalInput,
        Paste,
        Save,
        Undo,
    };
}
