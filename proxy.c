extern void DllInitialize_Impl();

extern void vSetDdrawflag_Impl();

extern void AlphaBlend_Impl();

extern void GradientFill_Impl();

extern void TransparentBlt_Impl();

static void __cdecl DllInitialize() {
    DllInitialize_Impl();
}

static void __cdecl vSetDdrawflag() {
    vSetDdrawflag_Impl();
}

static void __cdecl AlphaBlend() {
    AlphaBlend_Impl();
}

static void __cdecl GradientFill() {
    GradientFill_Impl();
}

static void __cdecl TransparentBlt() {
    TransparentBlt_Impl();
}