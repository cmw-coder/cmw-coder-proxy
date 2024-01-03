#pragma once

#include <cstdint>

namespace types {
    struct MemoryAddress {
        struct {
            struct {
                struct {
                    uint32_t pointer, offset1;
                } x;

                struct {
                    uint32_t windowHandle, funcYPosFromLine;
                } y;
            } dimension;

            struct {
                struct {
                    uint32_t line, character;
                } begin, current, end;
            } position;
        } caret;

        struct {
            uint32_t fileHandle, funcGetBufLine;
        } file;
    };
}
