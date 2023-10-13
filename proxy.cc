#include <format>

#include <types/ModuleProxy.h>
#include <utils/logger.h>

#include <windows.h>

using namespace std;
using namespace types;
using namespace utils;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    BYTE BlendOp;
    BYTE BlendFlags;
    BYTE SourceConstantAlpha;
    BYTE AlphaFormat;
} BLENDFUNCTION;

typedef USHORT COLOR16;

typedef struct {
    LONG x;
    LONG y;
    COLOR16 Red;
    COLOR16 Green;
    COLOR16 Blue;
    COLOR16 Alpha;
} *PTRIVERTEX;

void WINAPI vSetDdrawflag(void) {
#pragma comment(linker, "/EXPORT:vSetDdrawflag=_vSetDdrawflag@0")
    logger::log(format("Proxying {}", typeid(vSetDdrawflag).name()));
}

// https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-alphablend
BOOL WINAPI AlphaBlend(
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
#pragma comment(linker, "/EXPORT:AlphaBlend=_AlphaBlend@44")
    const auto remoteFunction = ModuleProxy::GetInstance()->getFunction<decltype(AlphaBlend)>("AlphaBlend");
    if (!remoteFunction) {
        logger::log("FATAL: Failed to get address of 'AlphaBlend'.");
        return false;
    }
    return remoteFunction(
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

// https://docs.microsoft.com/en-us/windows/win32/dlls/dllmain
BOOL WINAPI DllInitialize(
        HINSTANCE hinstDLL,
        DWORD fdwReason,
        LPVOID lpvReserved
) {
#pragma comment(linker, "/EXPORT:DllInitialize=_DllInitialize@12")
    (void) hinstDLL;
    (void) fdwReason;
    (void) lpvReserved;
    logger::log(format("Proxying {}", typeid(DllInitialize).name()));
//  DllMain(hinstDLL, fdwReason, lpvReserved);
    return TRUE;
}

// https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-gradientfill
BOOL WINAPI GradientFill(
        HDC hdc,
        PTRIVERTEX pVertex,
        ULONG nVertex,
        PVOID pMesh,
        ULONG nMesh,
        ULONG ulMode
) {
#pragma comment(linker, "/EXPORT:GradientFill=_GradientFill@24")
    const auto remoteFunction = ModuleProxy::GetInstance()->getFunction<decltype(GradientFill)>("GradientFill");
    if (!remoteFunction) {
        logger::log("FATAL: Failed to get address of 'GradientFill'.");
        return false;
    }
    return remoteFunction(hdc, pVertex, nVertex, pMesh, nMesh, ulMode);
}

// https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-transparentblt
BOOL WINAPI TransparentBlt(
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
#pragma comment(linker, "/EXPORT:TransparentBlt=_TransparentBlt@44")
    const auto remoteFunction = ModuleProxy::GetInstance()->getFunction<decltype(TransparentBlt)>("TransparentBlt");
    if (!remoteFunction) {
        logger::log("FATAL: Failed to get address of 'TransparentBlt'.");
        return false;
    }
    return remoteFunction(
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

#ifdef __cplusplus
}
#endif