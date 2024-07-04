#pragma once

#include <cstdint>

namespace models {
    struct MemoryAddress {
        enum class HandleType {
            File,
            // Project,
            Window
        };

        struct {
            uint32_t handle;

            struct {
                uint32_t base, param3;
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
            uint32_t handle, projectPath;
        } project;

        struct {
            struct {
                uint32_t base;
            } funcCreateSymbolList;

            struct {
                uint32_t base;
            } funcDestroySymbolList;

            struct {
                uint32_t base;
            } funcGetSymbolAddress;

            struct {
                uint32_t base, param3, param4;
            } funcGetSymbolChildren;

            struct {
                uint32_t base, param5;
            } funcGetSymbolHandle;

            struct {
                uint32_t base;
            } funcGetSymbolListNameAddress;

            struct {
                uint32_t base, param1Offset1;
            } funcGetSymbolNameByAddress;

            struct {
                uint32_t base;
            } funcGetSymbolNameByLine;

            struct {
                uint32_t base;
            } funcGetSymbolNameByRecord;

            struct {
                uint32_t base;
            } funcGetSymbolRecord;

            struct {
                uint32_t base;
            } funcInitializeSymbolBuffer;

            struct {
                uint32_t base;
            } funcInitializeSymbolList;

            struct {
                uint32_t base, param2, param3;
            } funcTransformSymbolNameToDeclaredType;
        } symbol;

        struct {
            uint32_t handle;

            struct {
                struct {
                    uint32_t base;
                } isSelecting;

                struct {
                    uint32_t base;
                } lineBegin, characterBegin, lineEnd, characterEnd;
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
