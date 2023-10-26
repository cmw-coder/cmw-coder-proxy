# Source Insight Reverse Engineering Requirements

## Source Insight Introduction

Home Page: [sourceinsight.com](http://www.sourceinsight.com/)

Most Commonly Used Versions:

- 3.50.0076
- 3.50.0086
- 4.00.0096
- 4.00.0099
- 4.00.0113
- 4.00.0116

Macro
System: [Macro Language Guide](https://www.sourceinsight.com/doc/v4/userguide/index.html#t=Manual%2FMacro_Language%2FMacro_Language.htm)

## Current Solution

- 4 Macros to get editor info, insert & cancel & accept completion
- `msimg32.dll` proxy, use `SetWindowsHookEx` to hook `WM_KILLFOCUS`, `WM_SETFOCUS`, `UM_KEYCODE` messages
- Read version-specific memory to detect caret position change
- Use `PostMessage` to send macro trigger message to Source Insight
- Node.js backend for prompt extraction and communicate with AIGC server

## Current Problems

- `msimg32.dll` proxy is not stable, some system just never loads it
- Hook `msimg32.dll` sometimes causes Source Insight to crash (When showing built in Intellisense)
- Macro System is very inefficient, and all program instances seem to share one single macro interpreter. This causes
  interpreter to crash when multiple instances are running at the same time.

## Requirements

- Read & Write editing file content directly from Source Insight memory
- Read Project Symbol database
- Completely abandon macro system