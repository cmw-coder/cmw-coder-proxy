#pragma once

#include <cstdint>

namespace models {
    struct MemoryAddress {
        struct {
            uint32_t handle;

            struct {
                uint32_t base;
            } funcDelBufLine;

            struct {
                uint32_t base;
            } funcGetBufLine;

            struct {
                uint32_t base, param1Base, param1Offset1;
            } funcGetBufName;

            struct {
                uint32_t base;
            } funcInsBufLine;

            struct {
                uint32_t base;
            } funcPutBufLine;
        } file;

        struct {
            uint32_t handle;

            struct {
                uint32_t offset1, offset2;
            } dataProjDir;
        } project;

        struct {
            uint32_t handle;

            struct {
                struct {
                    uint32_t base;
                } lineStart, characterStart, lineEnd, characterEnd;
            } dataSelection;

            struct {
                uint32_t offset1;
            } dataXPos;

            struct {
                uint32_t base;
            } dataYDim;

            struct {
                uint32_t base, param1Base;
            } funcSetBufSelText;

            struct {
                uint32_t base;
            } funcSetWndSel;

            struct {
                uint32_t base;
            } funcYPosFromLine;
        } window;
    };
}
