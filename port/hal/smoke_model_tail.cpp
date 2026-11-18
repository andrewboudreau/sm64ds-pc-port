// port/hal/smoke_model_tail.cpp -- host storage and name bridges the model
// family harnesses (smoke_model .. smoke_clsn) need since main's folds of GX,
// Model4, ModelComponents and G3i into whole TUs. Lane INT093F.
//
// (data_02086314 and the three after it are defined by the harnesses' own
// romdata.c; only the decorated spellings are bridged below.)
// The words below are plain zero-filled storage: no harness runs the paths
// that read them (texture-VRAM bank setup, the BG palette copy) with a meaning
// that matters, and the window build owns the real tables in hal/auto_bss.cpp
// and the arm9 data mount, which a harness does not link.

extern "C" {
// CP15::EnableDTCM: src/_ZN4CP1510EnableDTCMEv.cpp is two ARM coprocessor
// instructions (mrc / mcr), which no host compiler accepts; the window build
// takes the generated copy with the asm parked. The CP15 control register has
// no host meaning, so a harness gets the empty function.
void _ZN4CP1510EnableDTCMEv(void) {}
// MIi_CpuCopy16: the window build compiles src/MultiCopyHalf.c under the
// per-source rename MIi_CpuCopy16=MultiCopyHalf_rom and keeps the ROM name in
// hal/cxx_aliases.cpp with a PPU-audit census in front. A harness has no
// audit, so the ROM name is the straight call (same body as the census-less
// path).
void MultiCopyHalf_rom(const void *src, void *dst, int size);
void MIi_CpuCopy16(const void *src, void *dst, int size)
{ MultiCopyHalf_rom(src, dst, size); }
// cstd::strcmp: src/cstd6.cpp (the whole string TU) drags the OS tables; the
// harnesses want the comparison only.
int _ZN4cstd6strcmpEPKcS1_(const char *a, const char *b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return (unsigned char)*a - (unsigned char)*b;
}
// MI_Copy48B: src/Copy48Bytes.c is a hand-asm ldm/stm pair no host compiler
// accepts (the window build takes the generated copy); the function is a
// 48-byte block copy, one Matrix4x3.
void MI_Copy48B(const void *src, void *dst)
{ for (int i = 0; i < 12; ++i) ((unsigned int *)dst)[i] = ((const unsigned int *)src)[i]; }
// MI_Copy32B: the same hand-asm block copy at 32 bytes.
void MI_Copy32B(const void *src, void *dst)
{ for (int i = 0; i < 8; ++i) ((unsigned int *)dst)[i] = ((const unsigned int *)src)[i]; }
unsigned int data_020a60b4[8];
unsigned int data_020a60b8[8];
}

// THE GX BANK-STATE BLOCK, 0x020a6088..0x020a60a4, in ROM order: the same
// thirteen grouped sections hal/cxx_aliases.cpp lays out for the window build
// (read its note for why a split object is wrong), because tools/gxband_guard.py
// checks EVERY target's map and the harnesses now link the SetBankFor* family.
#define GXBANK(sec, name, size, algn)                             \
    __pragma(section(sec, read, write))                           \
    extern "C" __declspec(allocate(sec)) __declspec(align(algn))  \
    unsigned char name[size] = {0}

GXBANK(".dsstate$gxbank00", data_020a6088, 2, 4);
GXBANK(".dsstate$gxbank01", data_020a608a, 2, 2);
GXBANK(".dsstate$gxbank02", data_020a608c, 2, 2);
GXBANK(".dsstate$gxbank03", data_020a608e, 2, 2);
GXBANK(".dsstate$gxbank04", data_020a6090, 2, 2);
GXBANK(".dsstate$gxbank05", data_020a6092, 2, 2);
GXBANK(".dsstate$gxbank06", data_020a6094, 2, 2);
GXBANK(".dsstate$gxbank07", data_020a6096, 2, 2);
GXBANK(".dsstate$gxbank08", data_020a6098, 2, 2);
GXBANK(".dsstate$gxbank09", data_020a609a, 2, 2);
GXBANK(".dsstate$gxbank10", data_020a609c, 2, 2);
GXBANK(".dsstate$gxbank11", data_020a609e, 2, 2);
GXBANK(".dsstate$gxbank12", data_020a60a0, 4, 2);

#undef GXBANK

#pragma comment(linker, "/alternatename:?data_02086314@@3PAGA=_data_02086314")
#pragma comment(linker, "/alternatename:?data_02086324@@3PAGA=_data_02086324")
#pragma comment(linker, "/alternatename:?data_02086326@@3PAGA=_data_02086326")
#pragma comment(linker, "/alternatename:?data_02086328@@3PAGA=_data_02086328")
#pragma comment(linker, "/alternatename:?data_020a60b4@@3IA=_data_020a60b4")
#pragma comment(linker, "/alternatename:?data_020a60b8@@3IA=_data_020a60b8")
#pragma comment(linker, "/alternatename:?data_020a6088@@3UVramReg@@A=_data_020a6088")
// The GX::SetBankForTex forwarder under its flat spelling (_ZN2GX13SetBankForTexEt) is retired at
// catchup1 Stage C round 4: main has no GX::SetBankForTex any more (GX.cpp calls the C function
// GX_SetBankForTex(int) in GX_VramCnt.cpp directly), no harness object references the flat name,
// and keeping the body would drag GX_VramCnt.cpp into smoke_gx, which does not compile it.
#pragma comment(linker, "/alternatename:_GX_SetBankForTexPltt=?SetBankForTexPltt@GX@@YAXG@Z")
#pragma comment(linker, "/alternatename:_G3X_SetClearColor=?SetClearColor@G3X@@SAXGHHH_N@Z")
#pragma comment(linker, "/alternatename:_FX_GetDivResult=?fdiv_result@cstd@@YAHXZ")
#pragma comment(linker, "/alternatename:_FX_GetDivResultFx64c=?ldiv_result@cstd@@YA_JXZ")
// data_0208e87c was the port's old name for ModelBase's vtable at arm9
// 0x0208e87c; config calls it _ZTV11dExtModel_c and hal/model_host.cpp defines the
// storage under that name. The harness copies in port/unmatched/ now spell the
// new name, so the alias this file used to carry is gone (SEAMCLEAN1).

// catchup1 Stage C round 4 (SLICE4 rows): what d_ext_model.cpp, ModelComponents.cpp, GX.cpp and
// func_0204547c.cpp reach since main moved them onto C++ declarations.
//
// ::LoadFile(int) is only called by dExtModel_c::LoadAndSetFile, which no harness runs; the
// window build takes src/LoadFile.c and its file system. A harness that ever reaches it stops
// with a message instead of handing a null file to the model code.
// port_model_shrink_enabled is the window build's model-shrink switch (hal/level_boot.cpp); the
// harnesses check the ROM sizes, so it is off here.
#include <cstdio>
#include <cstdlib>
extern "C" void *LoadFile(int fileId)
{
    std::fprintf(stderr, "smoke harness: LoadFile(%d) reached; harnesses load no files\n", fileId);
    std::abort();
    return 0;
}
extern "C" int port_model_shrink_enabled(void) { return 0; }

// Storage the harness objects do not define (the window build's homes: hal/actor_vtables.cpp
// for data_020a0e68, data_0209ceec, data_0209cef4 and the overlay 1 template BMDs through
// hal/cxxname_bridge.cpp, hal/auto_bss.cpp for data_0209cef0). Zero-filled, sized as the ROM
// lays them out: data_020a0e68 is a Matrix4x3 scratch, the two template BMDs are 0x3c each.
extern "C" {
int data_020a0e68[12];
int data_0209cef0;
int data_0209ceec[4];
int data_0209cef4[4];
__declspec(align(4)) unsigned char data_ov001_020ad524[0x3c];
__declspec(align(4)) unsigned char data_ov001_020ad560[0x3c];
}

// The decorated spellings the C++ TUs now use, onto the C storage (model_host.cpp, romdata.c and
// the definitions above). FX_SinCosTable_ is the same pair hal/boot_hw.cpp carries for the window.
#pragma comment(linker, "/alternatename:?data_0209b3ec@@3UMatrix4x3@@A=_data_0209b3ec")
#pragma comment(linker, "/alternatename:?data_020a0ea0@@3PAXA=_data_020a0ea0")
#pragma comment(linker, "/alternatename:?data_0209cef8@@3PAHA=_data_0209cef8")
#pragma comment(linker, "/alternatename:?data_0209cefc@@3PAHA=_data_0209cefc")
#pragma comment(linker, "/alternatename:?data_0209cef0@@3HA=_data_0209cef0")
#pragma comment(linker, "/alternatename:?data_0208e738@@3HA=_data_0208e738")
#pragma comment(linker, "/alternatename:?data_0209cef4@@3PAUdExtShadowModel_c@@A=_data_0209cef4")
#pragma comment(linker, "/alternatename:?data_0209ceec@@3EA=_data_0209ceec")
#pragma comment(linker, "/alternatename:?data_ov001_020ad524@@3UBMD_File@@A=_data_ov001_020ad524")
#pragma comment(linker, "/alternatename:?data_ov001_020ad560@@3UBMD_File@@A=_data_ov001_020ad560")
#pragma comment(linker, "/alternatename:?data_02099f88@@3UVecFx32@@A=_data_02099f88")
#pragma comment(linker, "/alternatename:?data_020a4bd4@@3IA=_data_020a4bd4")
#pragma comment(linker, "/alternatename:?data_02082190@@3QBHB=_data_02082190")
#pragma comment(linker, "/alternatename:?data_020a4bd0@@3PAXA=_data_020a4bd0")
#pragma comment(linker, "/alternatename:?data_020a4bbc@@3EA=_data_020a4bbc")
#pragma comment(linker, "/alternatename:?data_02099f94@@3PAGA=_data_02099f94")
#pragma comment(linker, "/alternatename:?data_020a4bc0@@3FA=_data_020a4bc0")
#pragma comment(linker, "/alternatename:?data_020a4bc4@@3FA=_data_020a4bc4")
#pragma comment(linker, "/alternatename:?data_020a60b4@@3HA=_data_020a60b4")
#pragma comment(linker, "/alternatename:?data_020a60b8@@3HA=_data_020a60b8")
#pragma comment(linker, "/alternatename:?data_02086314@@3QBGB=_data_02086314")
#pragma comment(linker, "/alternatename:?data_02086324@@3QBUGXTexStartAddr@@B=_data_02086324")
#pragma comment(linker, "/alternatename:?data_02099f80@@3UB3@@A=_data_02099f80")
#pragma comment(linker, "/alternatename:?data_02099f84@@3UB3c@@B=_data_02099f84")
#pragma comment(linker, "/alternatename:_FX_SinCosTable_=_data_02082214")
#pragma comment(linker, "/alternatename:?FX_SinCosTable_@@3QBFB=_data_02082214")
