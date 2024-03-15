#include <format>

#include <components/ModuleProxy.h>
#include <utils/logger.h>

#include <windows.h>

using namespace components;
using namespace std;
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
} TRIVERTEX;

void WINAPI vSetDdrawflag(void) {
#pragma comment(linker, "/EXPORT:vSetDdrawflag=_vSetDdrawflag@0")
    logger::info(format("Proxying {}", typeid(vSetDdrawflag).name()));
}

// https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-alphablend
BOOL WINAPI AlphaBlend(
    const HDC__* hdcDest,
    const int xoriginDest,
    const int yoriginDest,
    const int wDest,
    const int hDest,
    const HDC__* hdcSrc,
    const int xoriginSrc,
    const int yoriginSrc,
    const int wSrc,
    const int hSrc,
    const BLENDFUNCTION ftn
) {
#pragma comment(linker, "/EXPORT:AlphaBlend=_AlphaBlend@44")
    const auto remoteFunction = ModuleProxy::GetInstance()->getFunction<decltype(AlphaBlend)>("AlphaBlend");
    if (!remoteFunction) {
        logger::error("Failed to get address of 'AlphaBlend'.");
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

// https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-gradientfill
BOOL WINAPI GradientFill(
    const void* hdc,
    const TRIVERTEX* pVertex,
    const ULONG nVertex,
    const void* pMesh,
    const ULONG nMesh,
    const ULONG ulMode
) {
#pragma comment(linker, "/EXPORT:GradientFill=_GradientFill@24")
    const auto remoteFunction = ModuleProxy::GetInstance()->getFunction<decltype(GradientFill)>("GradientFill");
    if (!remoteFunction) {
        logger::error("Failed to get address of 'GradientFill'.");
        return false;
    }
    return remoteFunction(hdc, pVertex, nVertex, pMesh, nMesh, ulMode);
}

// https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-transparentblt
BOOL WINAPI TransparentBlt(
    const void* hdcDest,
    const int xoriginDest,
    const int yoriginDest,
    const int wDest,
    const int hDest,
    const void* hdcSrc,
    const int xoriginSrc,
    const int yoriginSrc,
    const int wSrc,
    const int hSrc,
    const UINT crTransparent
) {
#pragma comment(linker, "/EXPORT:TransparentBlt=_TransparentBlt@44")
    const auto remoteFunction = ModuleProxy::GetInstance()->getFunction<decltype(TransparentBlt)>("TransparentBlt");
    if (!remoteFunction) {
        logger::error("Failed to get address of 'TransparentBlt'.");
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
