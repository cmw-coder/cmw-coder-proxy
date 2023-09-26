typedef int BOOL;
typedef unsigned char BYTE;
typedef long LONG;
typedef void *LPVOID, *PVOID;
typedef unsigned int UINT;
typedef unsigned long DWORD, ULONG;
typedef unsigned short COLOR16;

typedef struct _BLENDFUNCTION {
    BYTE BlendOp;
    BYTE BlendFlags;
    BYTE SourceConstantAlpha;
    BYTE AlphaFormat;
} BLENDFUNCTION;

typedef struct _TRIVERTEX {
    LONG x;
    LONG y;
    COLOR16 Red;
    COLOR16 Green;
    COLOR16 Blue;
    COLOR16 Alpha;
} *PTRIVERTEX;

typedef struct HDC__ {
    int unused;
} *HDC;

typedef struct HINSTANCE__ {
    int unused;
} *HINSTANCE;

extern BOOL DllInitialize_Impl(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved);

extern void vSetDdrawflag_Impl();

extern BOOL AlphaBlend_Impl(
        HDC hdcDest,
        int xoriginDest,
        int yoriginDest,
        int wDest,
        int hDest,
        HDC hdcSrc,
        int xoriginSrc,
        int yoriginSrc,
        int wSrc,
        int hSrc,
        BLENDFUNCTION ftn
);

extern BOOL GradientFill_Impl(
        HDC hdc,
        PTRIVERTEX pVertex,
        ULONG nVertex,
        PVOID pMesh,
        ULONG nMesh,
        ULONG ulMode
);

extern BOOL TransparentBlt_Impl(
        HDC hdcDest,
        int xoriginDest,
        int yoriginDest,
        int wDest,
        int hDest,
        HDC hdcSrc,
        int xoriginSrc,
        int yoriginSrc,
        int wSrc,
        int hSrc,
        UINT crTransparent
);

__declspec(dllexport) BOOL __cdecl DllInitialize(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    return DllInitialize_Impl(hinstDLL, fdwReason, lpReserved);
}

__declspec(dllexport) void vSetDdrawflag() {
    vSetDdrawflag_Impl();
}

__declspec(dllexport) BOOL __cdecl AlphaBlend(
        HDC hdcDest,
        int xoriginDest,
        int yoriginDest,
        int wDest,
        int hDest,
        HDC hdcSrc,
        int xoriginSrc,
        int yoriginSrc,
        int wSrc,
        int hSrc,
        BLENDFUNCTION ftn
) {
    return AlphaBlend_Impl(
            hdcDest,
            xoriginDest,
            yoriginDest,
            wDest,
            hDest,
            hdcSrc,
            xoriginSrc,
            yoriginSrc,
            wSrc,
            hSrc,
            ftn
    );
}

__declspec(dllexport) BOOL __cdecl GradientFill(
        HDC hdc,
        PTRIVERTEX pVertex,
        ULONG nVertex,
        PVOID pMesh,
        ULONG nMesh,
        ULONG ulMode
) {
    return GradientFill_Impl(hdc, pVertex, nVertex, pMesh, nMesh, ulMode);
}

__declspec(dllexport) BOOL __cdecl TransparentBlt(
        HDC hdcDest,
        int xoriginDest,
        int yoriginDest,
        int wDest,
        int hDest,
        HDC hdcSrc,
        int xoriginSrc,
        int yoriginSrc,
        int wSrc,
        int hSrc,
        UINT crTransparent
) {
    return TransparentBlt_Impl(
            hdcDest,
            xoriginDest,
            yoriginDest,
            wDest,
            hDest,
            hdcSrc,
            xoriginSrc,
            yoriginSrc,
            wSrc,
            hSrc,
            crTransparent
    );
}