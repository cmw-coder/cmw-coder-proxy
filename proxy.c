#include <wingdi.h>
#include <windef.h>

//struct HDC__ {
//    int unused;
//};
//typedef struct HDC__ *HDC;

typedef unsigned char BYTE;
typedef long LONG;

typedef struct _BLENDFUNCTION {
    BYTE BlendOp;
    BYTE BlendFlags;
    BYTE SourceConstantAlpha;
    BYTE AlphaFormat;
} BLENDFUNCTION, *PBLENDFUNCTION;

typedef struct _TRIVERTEX
{
    LONG    x;
    LONG    y;
    COLOR16 Red;
    COLOR16 Green;
    COLOR16 Blue;
    COLOR16 Alpha;
}TRIVERTEX,*PTRIVERTEX,*LPTRIVERTEX;

extern void DllInitialize_Impl();

extern void vSetDdrawflag_Impl();

extern int AlphaBlend_Impl(
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

extern int GradientFill_Impl(
        HDC hdc,
        PTRIVERTEX pVertex,
        ULONG nVertex,
        PVOID pMesh,
        ULONG nMesh,
        ULONG ulMode
);

extern int TransparentBlt_Impl(
        _In_ HDC hdcDest,
        _In_ int xoriginDest,
        _In_ int yoriginDest,
        _In_ int wDest,
        _In_ int hDest,
        _In_ HDC hdcSrc,
        _In_ int xoriginSrc,
        _In_ int yoriginSrc,
        _In_ int wSrc,
        _In_ int hSrc,
        _In_ UINT crTransparent
);

__declspec(dllexport) void __cdecl DllInitialize() {
    DllInitialize_Impl();
}

__declspec(dllexport) void __cdecl vSetDdrawflag() {
    vSetDdrawflag_Impl();
}

__declspec(dllexport) int __cdecl AlphaBlend(
        _In_ HDC hdcDest,
        _In_ int xoriginDest,
        _In_ int yoriginDest,
        _In_ int wDest,
        _In_ int hDest,
        _In_ HDC hdcSrc,
        _In_ int xoriginSrc,
        _In_ int yoriginSrc,
        _In_ int wSrc,
        _In_ int hSrc,
        _In_ BLENDFUNCTION ftn
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
        _In_ HDC hdc,
        _In_reads_(nVertex) PTRIVERTEX pVertex,
        _In_ ULONG nVertex,
        _In_ PVOID pMesh,
        _In_ ULONG nMesh,
        _In_ ULONG ulMode
) {
    return GradientFill_Impl(hdc, pVertex, nVertex, pMesh, nMesh, ulMode);
}

__declspec(dllexport) BOOL __cdecl TransparentBlt(
        _In_ HDC hdcDest,
        _In_ int xoriginDest,
        _In_ int yoriginDest,
        _In_ int wDest,
        _In_ int hDest,
        _In_ HDC hdcSrc,
        _In_ int xoriginSrc,
        _In_ int yoriginSrc,
        _In_ int wSrc,
        _In_ int hSrc,
        _In_ UINT crTransparent
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