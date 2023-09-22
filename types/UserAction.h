#pragma once

namespace types {
    enum class UserAction {
        Idle = -1,
        Accept,
        DeleteBackward,
        DeleteForward,
        ModifyLine,
        Navigate,
        Normal,
    };
}