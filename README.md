# si-coding-hook

## Overview

This is a simple Windows DLL that hooks into the Windows message queue and listens for WM_KILLFOCUS, WM_SETFOCUS, and
UM_KEYCODE messages. When it receives a WM_KILLFOCUS message, it will update the registry to indicate that the user is
no longer focused on the application. When it receives a WM_SETFOCUS message, it will update the registry to indicate
that the user is focused on the application. When it receives a UM_KEYCODE message, it will update the registry to
indicate that the user has pressed a key.

## Design

```mermaid
flowchart LR
    programStart([Program Start]) --> loadDll[Load DLLs] -.-> lookupSystemMsimg32[Lookup \n system msimg32.dll]
    style lookupSystemMsimg32 stroke-dasharray: 5 5
    loadDll == Injected ==> lookupCustomMsimg32[Lookup \n custom msimg.dll]
    lookupCustomMsimg32 ==> sideLoadSystemMsimg32[Side-load \n system msimg32.dll]
    lookupCustomMsimg32 --> startThreadLoop[Start thread loop]
    lookupCustomMsimg32 --> setWindowsHookEx[SetWindowsHookEx]
    lookupSystemMsimg32 -.-> sideLoadSystemMsimg32 --> provideOriginalFunctions[Provide original functions]

    subgraph Hook Procedure
        setWindowsHookEx <-- Event Loop --> checkMessageType{{Check \n message type}}
        checkMessageType --> killFocus[WM_KILLFOCUS] --> updateFocusState[Update focus state]
        checkMessageType --> setFocus[WM_SETFOCUS] --> updateFocusState
        checkMessageType --> userKeycode[UM_KEYCODE] --> updateRegistry[Update registry]
    end

    subgraph Main Thread Loop
        startThreadLoop -- Run every 500ms ---> checkFocusState{{Check focus state}}
        checkFocusState -- True --> sendTriggerMessage[Send trigger message]
        checkFocusState -- False --> delay[Delay 2000ms]
    end

    updateRegistry -.-> sourceInsight([Source Insight])
    sendTriggerMessage -.-> sourceInsight
```