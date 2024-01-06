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
                    uint32_t windowHandle;

                    struct {
                        uint32_t funcAddress;
                    } funcYPosFromLine;
                } y;
            } dimension;

            struct {
                struct {
                    uint32_t line, character;
                } begin, current, end;
            } position;
        } caret;

        struct {
            uint32_t fileHandle;

            struct {
                uint32_t funcAddress;
            } funcDelBufLine;

            struct {
                uint32_t funcAddress;
            } funcGetBufLine;

            struct {
                uint32_t funcAddress, param1Offset1, param1Offset2;
            } funcGetBufName;

            struct {
                uint32_t funcAddress;
            } funcInsBufLine;

            struct {
                uint32_t funcAddress;
            } funcPutBufLine;
        } file;

        struct {
            uint32_t windowHandle;

            struct {
                uint32_t funcAddress, param1;
            } funcSetBufSelText;

            struct {
                uint32_t funcAddress;
            } funcSetWndSel;
        } window;
    };
}
