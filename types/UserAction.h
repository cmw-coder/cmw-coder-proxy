#pragma once

namespace types {
    enum class UserAction {
        Idle = -1,
        Normal = 0,
        Navigate = 1,
        DeleteBackward = 2,
        ModifyLine = 3,
        Accept,
        DeleteForward,
    };
}