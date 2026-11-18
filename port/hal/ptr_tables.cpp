// Two ROM tables that romdata.py was emitting as raw bytes, hosted.
//
// ---- the bug class ---------------------------------------------------------
//
// port/tools/romdata.py hands the port the ROM's own bytes for every symbol in
// its NAMED list. That is right for constants and wrong, silently and for a
// long time, for any symbol whose bytes contain a RELOCATED word: a relocated
// word holds a DS address, and a DS address called through on the host is a
// jump into unmapped memory. data_02099fb4 / data_02099fbc were exactly this
// and killed a play session about ten thousand frames in (see
// hal/particle_vtable.cpp).
//
// port/tools/ptr_audit.py is the sweep for the rest. It intersects NAMED with
// config/arm9/relocs.txt and reports every named symbol carrying a relocation.
// Of the forty it flagged as "code pointers, named by no host TU", every one
// belongs to one of the two tables below -- and both are provably indexed and
// CALLED by matched code that is already in the link.
//
// The reason this hid so well is worth writing down: the callees are matched
// src that is in NO SLICE, so they were never compiled. "The symbol is not in
// the build" is evidence FOR this bug, not against it, because the only thing
// naming those functions was a table of bytes. port/slice_ptrsweep.txt is that
// list; nothing in it is new decomp.
//
// ---- table 1: fsi_default_command, the backup-media command table ----------------
//
// src/func_0205c5e4.c, the engine's per-command entry:
//
//     if (ret == 7)
//         ret = fsi_default_command[idx](self);
//
// -- nine plain C function pointers, indexed by the command id its callers
// pass (FSi_ExecuteSyncCommand passes self+0x10, FSi_ExecuteAsyncCommand passes self->f10, and
// the handlers themselves re-enter with 2, 3 and 7). Emitted as ROM bytes the
// port's copy held nine DS addresses.
//
// ---- table 2: the 39 records at 0x02086bc0.., the camera script commands ---
//
// src/func_02008550.cpp seeds sScriptOps$2629[39] from them once and then
// dispatches:
//
//     (cam->*sScriptOps$2629[msg[6]])(msg + 7, beg, end)
//
// and its one caller is src/ProcessKuppaScript.cpp, command 4, with
// gCamera -- the Camera -- as the object. That is the camera half of the
// kuppa/cutscene script language: _ZN11KuppaScript7ProcessEv is reached from the level
// boot (Stage::LoadClsnAndObjects calls _ZN11KuppaScript19ContinueIfNecessaryEv) and
// from the star-get cutscene (func_ov002_020c7ff8 -> _ZN9dCamera_c18StartDanceCutsceneEji ->
// _ZN11KuppaScript18StartDanceCutsceneEi -> _ZN11KuppaScript3RunEPh).
//
// Each record is the ROM's 8-byte mwcc pointer-to-member pair, {code address,
// this-delta}, and every delta in the run is zero. They sit INTERLEAVED with
// the camera STATE machine's 38 pairs in the same address run -- those are the
// ones hal/camera_states.cpp bakes in, at 0x02086bc8, 0x02086bd0, 0x02086be0
// and so on. Same region, different table, different consumer.
//
// The dispatch itself cannot survive the ABI: MSVC's PMF for a complete
// single-inheritance class is a 4-byte __thiscall address, and the callees are
// C-linkage cdecl functions taking the camera as their first stack argument.
// So the consumer, src/func_02008550.cpp, goes through hostgen's MG_PMF_CALL
// patch (lane shadow-A retired the host copy that used to stand in for it):
// the 39 seeding assignments copy word 0 out of each record exactly as the
// ROM's copy does, and the one call reads it back as a plain function. Same
// hop hal/camera_states.cpp already makes for the state machine next door.
//
// ---- how to re-derive this file --------------------------------------------
//
// Every entry below quotes its own line out of config/arm9/relocs.txt. Nothing
// here is a guess about layout: `from` is the word the ROM relocates and `to`
// is where it points, and the symbol names come straight from
// config/arm9/symbols.txt. Both tables are removed from romdata.py's NAMED
// list in exchange -- putting either name back there would define the symbol
// twice and, if the byte copy won, restore the crash.
//
// port_ptr_tables_check() re-asserts the whole shape before the first frame.
#include <cstdio>
#include <cstdlib>

#include "dsstate_seg.h"

extern "C" {

/* THE ONE PRIMITIVE THE CALLEES REACH THAT HAS NO C BODY. src/CpuCopy8.c is
   an asm-primitive TU: a hand-tuned unaligned block copy that aligns the
   destination with an edge-preserving read-modify-write so no 8-bit store is
   ever issued. That trick exists for the DS's VRAM bus, which cannot take a
   byte write; on the host a plain byte copy is the same function. Its only
   caller in this slice is FSi_GetPathCommand, and it is not in
   port/slice_ptrsweep.txt for exactly this reason.
   PORT_HOST_ABI: ARM asm primitive (unaligned block copy), MSVC cannot assemble. */
void MI_CpuCopy8(const void *src, void *dst, unsigned int size)
{
    const unsigned char *s = (const unsigned char *)src;
    unsigned char *d = (unsigned char *)dst;
    while (size--)
        *d++ = *s++;
}

// ---- table 1: fsi_default_command ------------------------------------------------
//
// The handlers are declared here the way the table uses them -- one argument,
// an int result. FSi_WriteFileCommand and FSi_ReadFileCommand are decompiled `void` and let
// their result ride out of the tail call they end in, which is an ARM
// convention the host does not share; they are in the port's ride-through
// catalog and the two slots are typed through anyway, because the alternative
// is not calling them at all.
unsigned int FSi_CloseFileCommand(char *);
unsigned int FSi_OpenFileDirectCommand(char *);
unsigned int FSi_OpenFileFastCommand(char *);
unsigned int FSi_GetPathCommand(char *);
unsigned int FSi_FindPathCommand(char *);
unsigned int FSi_ReadDirCommand(char *);
unsigned int FSi_SeekDirCommand(char *);
unsigned int FSi_WriteFileCommand(char *);
unsigned int FSi_ReadFileCommand(char *);

/* from:0x02086758 to:0x0205c448 | 0x0208675c to:0x0205c410
   from:0x02086760 to:0x0205c378 | 0x02086764 to:0x0205c264
   from:0x02086768 to:0x0205c048 | 0x0208676c to:0x0205bc88
   from:0x02086770 to:0x0205bc0c | 0x02086774 to:0x0205bbe4
   from:0x02086778 to:0x0205bbdc */
DSSTATE_BEGIN
unsigned int (*fsi_default_command[9])(char *) = {
    FSi_ReadFileCommand,
    FSi_WriteFileCommand,
    FSi_SeekDirCommand,
    FSi_ReadDirCommand,
    FSi_FindPathCommand,
    FSi_GetPathCommand,
    FSi_OpenFileFastCommand,
    FSi_OpenFileDirectCommand,
    FSi_CloseFileCommand,
};

// ---- table 2: the 39 camera-script records ---------------------------------
//
// Declared through the widest shape the dispatch can present: the ROM enters
// them with the camera in r0 and (msg + 7, beg, end) in r1..r3, and the
// individual handlers spell as many of those as they read. Host-side that is
// one cdecl signature, and the extra arguments a one-parameter handler ignores
// are the caller's to clean up.
typedef int (*CamScriptFn)(void *cam, unsigned char *arg, int beg, int end);

/* catchup1 (SYM4): decomp main defines all 39 ops at C++ linkage in
   src/engine/view/d_camera.cpp and d_camera_dtor.cpp, most under new names
   (config/arm9/symbols.txt, same addresses), each with its own arity. They are
   declared exactly as main defines them so MSVC decorates the same names;
   the record keeps only the address, as before. */
}  /* extern "C": the ops below are C++ at TARGET */
struct dCamera_c;
int CutsceneGrandStarFlyAppCvars(dCamera_c *);
int CutsceneGrandStarFlyMarioOffscreen(dCamera_c *);
int CutsceneGrandStarFlyMoveToMario(dCamera_c *);
int CutsceneGrandStarFlyStart(dCamera_c *);
int CutsceneGrandStarMoveCvar2(dCamera_c *);
int CutsceneGrandStarApproachMario(dCamera_c *);
int CutsceneGrandStarAccelCvar2(dCamera_c *);
int CutsceneGrandStarMarioJump(dCamera_c *);
int CutsceneGrandStarFocusMario(dCamera_c *);
int CutsceneGrandStarFrontOfMario(dCamera_c *);
int CutsceneGrandStarStart(dCamera_c *);
int CutsceneKeyDanceJumpCvar(dCamera_c *, unsigned char *);
int CutsceneDanceCloseupFlyAbove(dCamera_c *);
int CutsceneDanceCloseupStart(dCamera_c *);
int CutsceneDanceShakeFov(dCamera_c *, unsigned char *);
int func_020078c4(dCamera_c *, unsigned char *);
int RotateAndMoveVec3f(dCamera_c *, unsigned char *);
int CutscenePanCvar9(dCamera_c *, unsigned char *, int);
int func_02007a8c(dCamera_c *, unsigned char *);
int FocusInFrontOfMario(dCamera_c *, unsigned char *);
int func_02007b98(dCamera_c *, unsigned char *);
int CutsceneDanceFlyAwayStart(dCamera_c *);
int func_02007e5c(dCamera_c *, unsigned char *);
int func_02007f14(dCamera_c *, unsigned char *);
int func_02007fcc(dCamera_c *, unsigned char *);
int func_02008008(dCamera_c *, unsigned char *);
int func_02008028(dCamera_c *);
int func_02008080(dCamera_c *);
int func_020080b0(dCamera_c *);
int func_020080f0(dCamera_c *);
int func_0200814c(dCamera_c *);
int func_02008200(dCamera_c *, unsigned char *, int);
int func_020082a0(dCamera_c *, unsigned char *, int);
int func_020082f8(dCamera_c *, unsigned char *, int, int);
int func_0200840c(dCamera_c *, unsigned char *);
int func_02008450(dCamera_c *, unsigned char *);
int func_02008474(dCamera_c *, unsigned char *, int, int);
int func_020084b0(dCamera_c *, unsigned char *, int, int);
int func_02008500(dCamera_c *, unsigned char *);
extern "C" {

/* Each record keeps the ROM's own two-word shape, {handler, this-delta}, so
   the host copy of _Z16RunCameraCommandP9dCamera_cPhii reads word 0 the same way the matched source
   copies the pair. The index in the comment is the sScriptOps$2629 slot, which
   is the script command byte. */
/* [ 0] from:0x02086c80 to:0x02008500 */
void *data_02086c80[2] = { (void *)&func_02008500, 0 };
/* [ 1] from:0x02086d80 to:0x020084b0 */
void *data_02086d80[2] = { (void *)&func_020084b0, 0 };
/* [ 2] from:0x02086c60 to:0x02008474 */
void *data_02086c60[2] = { (void *)&func_02008474, 0 };
/* [ 3] from:0x02086c90 to:0x02008450 */
void *data_02086c90[2] = { (void *)&func_02008450, 0 };
/* [ 4] from:0x02086bd8 to:0x0200840c */
void *data_02086bd8[2] = { (void *)&func_0200840c, 0 };
/* [ 5] from:0x02086d18 to:0x020082f8 */
void *data_02086d18[2] = { (void *)&func_020082f8, 0 };
/* [ 6] from:0x02086d98 to:0x020082a0 */
void *data_02086d98[2] = { (void *)&func_020082a0, 0 };
/* [ 7] from:0x02086c88 to:0x02008200 */
void *data_02086c88[2] = { (void *)&func_02008200, 0 };
/* [ 8] from:0x02086d00 to:0x0200814c */
void *data_02086d00[2] = { (void *)&func_0200814c, 0 };
/* [ 9] from:0x02086bc0 to:0x020080f0 */
void *data_02086bc0[2] = { (void *)&func_020080f0, 0 };
/* [10] from:0x02086e30 to:0x020080b0 */
void *data_02086e30[2] = { (void *)&func_020080b0, 0 };
/* [11] from:0x02086ce8 to:0x02008080 */
void *data_02086ce8[2] = { (void *)&func_02008080, 0 };
/* [12] from:0x02086e38 to:0x02008028 */
void *data_02086e38[2] = { (void *)&func_02008028, 0 };
/* [13] from:0x02086e20 to:0x02008008 */
void *data_02086e20[2] = { (void *)&func_02008008, 0 };
/* [14] from:0x02086cd0 to:0x02007fcc */
void *data_02086cd0[2] = { (void *)&func_02007fcc, 0 };
/* [15] from:0x02086e08 to:0x02007f14 */
void *data_02086e08[2] = { (void *)&func_02007f14, 0 };
/* [16] from:0x02086df0 to:0x02007e5c */
void *data_02086df0[2] = { (void *)&func_02007e5c, 0 };
/* [17] from:0x02086cb8 to:0x02007d58 */
void *data_02086cb8[2] = { (void *)&CutsceneDanceFlyAwayStart, 0 };
/* [18] from:0x02086dd8 to:0x02007b98 */
void *data_02086dd8[2] = { (void *)&func_02007b98, 0 };
/* [19] from:0x02086dc0 to:0x02007b0c */
void *data_02086dc0[2] = { (void *)&FocusInFrontOfMario, 0 };
/* [20] from:0x02086c20 to:0x02007a8c */
void *data_02086c20[2] = { (void *)&func_02007a8c, 0 };
/* [21] from:0x02086da0 to:0x020079ac */
void *data_02086da0[2] = { (void *)&CutscenePanCvar9, 0 };
/* [22] from:0x02086d60 to:0x02007910 */
void *data_02086d60[2] = { (void *)&RotateAndMoveVec3f, 0 };
/* [23] from:0x02086d88 to:0x020078c4 */
void *data_02086d88[2] = { (void *)&func_020078c4, 0 };
/* [24] from:0x02086cf8 to:0x02007870 */
void *data_02086cf8[2] = { (void *)&CutsceneDanceShakeFov, 0 };
/* [25] from:0x02086d68 to:0x020077f0 */
void *data_02086d68[2] = { (void *)&CutsceneDanceCloseupStart, 0 };
/* [26] from:0x02086c78 to:0x02007758 */
void *data_02086c78[2] = { (void *)&CutsceneDanceCloseupFlyAbove, 0 };
/* [27] from:0x02086d40 to:0x02007698 */
void *data_02086d40[2] = { (void *)&CutsceneKeyDanceJumpCvar, 0 };
/* [28] from:0x02086d38 to:0x0200762c */
void *data_02086d38[2] = { (void *)&CutsceneGrandStarStart, 0 };
/* [29] from:0x02086d20 to:0x020075b4 */
void *data_02086d20[2] = { (void *)&CutsceneGrandStarFrontOfMario, 0 };
/* [30] from:0x02086c50 to:0x02007548 */
void *data_02086c50[2] = { (void *)&CutsceneGrandStarFocusMario, 0 };
/* [31] from:0x02086d08 to:0x0200751c */
void *data_02086d08[2] = { (void *)&CutsceneGrandStarMarioJump, 0 };
/* [32] from:0x02086cf0 to:0x020074e8 */
void *data_02086cf0[2] = { (void *)&CutsceneGrandStarAccelCvar2, 0 };
/* [33] from:0x02086c38 to:0x02007484 */
void *data_02086c38[2] = { (void *)&CutsceneGrandStarApproachMario, 0 };
/* [34] from:0x02086cd8 to:0x02007450 */
void *data_02086cd8[2] = { (void *)&CutsceneGrandStarMoveCvar2, 0 };
/* [35] from:0x02086cc0 to:0x02007414 */
void *data_02086cc0[2] = { (void *)&CutsceneGrandStarFlyStart, 0 };
/* [36] from:0x02086cb0 to:0x020073a0 */
void *data_02086cb0[2] = { (void *)&CutsceneGrandStarFlyMoveToMario, 0 };
/* [37] from:0x02086ca8 to:0x020072c4 */
void *data_02086ca8[2] = { (void *)&CutsceneGrandStarFlyMarioOffscreen, 0 };
/* [38] from:0x02086ca0 to:0x020071e0 */
void *data_02086ca0[2] = { (void *)&CutsceneGrandStarFlyAppCvars, 0 };

/* The roster the checker and the host copy of _Z16RunCameraCommandP9dCamera_cPhii both walk, in
   sScriptOps$2629 index order -- which is the order src/func_02008550.cpp
   assigns them, not address order. */
/* `extern` spelled on the definition on purpose: a const array at namespace
   scope has INTERNAL linkage in C++ even inside extern "C", and without this
   the host copy of _Z16RunCameraCommandP9dCamera_cPhii cannot see it. */
extern void *const *const port_cam_script_records[39];
void *const *const port_cam_script_records[39] = {
    data_02086c80, data_02086d80, data_02086c60, data_02086c90,
    data_02086bd8, data_02086d18, data_02086d98, data_02086c88,
    data_02086d00, data_02086bc0, data_02086e30, data_02086ce8,
    data_02086e38, data_02086e20, data_02086cd0, data_02086e08,
    data_02086df0, data_02086cb8, data_02086dd8, data_02086dc0,
    data_02086c20, data_02086da0, data_02086d60, data_02086d88,
    data_02086cf8, data_02086d68, data_02086c78, data_02086d40,
    data_02086d38, data_02086d20, data_02086c50, data_02086d08,
    data_02086cf0, data_02086c38, data_02086cd8, data_02086cc0,
    data_02086cb0, data_02086ca8, data_02086ca0,
};


// ---- table 3: sDanceCutsceneTable, the Kuppa/star-get script-pointer table -------
//
// src/func_0200ee8c.c, the star-get camera-script launcher:
//
//     _ZN11KuppaScript3RunEPh(sDanceCutsceneTable[arg0]);
//
// reached from func_ov002_020c7ff8 (star-collect Player state; the s==3 boss
// case passes index 5) -> _ZN9dCamera_c18StartDanceCutsceneEji -> _ZN11KuppaScript18StartDanceCutsceneEi. Each slot is a
// RELOCATED word pointing at a Kuppa-script bytecode blob; byte-copied by
// romdata.py the port's sDanceCutsceneTable[i] held six raw DS addresses, and
// _ZN11KuppaScript3RunEPh seated gRunningKuppaScript with one, so _ZN11KuppaScript7ProcessEv faulted
// walking a garbage pointer (`while (*s != 0) { ...; s += *s; }`).
//
// Same bug class as tables 1 and 2. The difference is the targets are DATA, not
// code: the six words point at five unique script blobs (slots 2 and 4 share
// 0x0208776c). No relocation lands inside any blob, so each is byte-hosted
// verbatim from arm9_dec.bin. Each blob is a run of length-prefixed records
// terminated by a 0 byte -- _ZN11KuppaScript7ProcessEv's own walk -- so its size is
// exactly the byte offset of that terminator plus one, never a boundary guess:
//   0x02088388 -> 69, 0x020888f0 -> 92, 0x0208776c -> 44 (ends at the next
//   symbol sEndingMarioToPeachFocusPoints), 0x020889b0 -> 103, 0x02088b14 -> 139.
//
// sDanceCutsceneTable is removed from romdata.py's NAMED list in exchange; putting
// it back would define the symbol twice and, if the byte copy won, restore the
// crash. func_0200ee8c.c reaches it as `extern void *sDanceCutsceneTable[]` (C
// linkage), which this definition satisfies the same way table 1's array does.
//
/* RETIRED at catchup1 Stage D round 3: src/KuppaScript.cpp now defines
   sDanceCutsceneTable itself, six host pointers to its own script arrays
   (sCutsceneDanceRotate among them, so IsDanceRotateCutsceneRunning still
   compares one object). The byte-hosted blobs and this file's copy were a
   second storage for ROM 0x020876e4; the C name now follows the decomp table
   (hal/cxx_aliases.cpp) and port_ptr_tables_check below still proves every
   slot is a host pointer on frame 0. */
extern void *sDanceCutsceneTable[];
// ---- table 4: data_020756f0, the twelve shared-model SharedFilePtrs --------
//
// src/_ZN5Stage13InitResourcesEv.cpp, between LoadGraphics2D and LoadModel:
//
//     for (i = 0; i < 0xC; i++) dExtSimpleModel_c::LoadFile(data_020756f0[i]);
//
// Twelve relocated words -- the coin, the mushroom, the shared pickup models --
// loaded once per level entry so the classes that read SharedFilePtr::filePtr
// straight out (OneUpMushroom::InitResources for types 11 and 12, Coin for its
// first two kinds) find a loaded file with no LoadFile of their own in front.
//
// SAME BUG CLASS AS TABLES 1 TO 3, WITH ONE DIFFERENCE WORTH SAYING: the
// twelve targets are not arm9 at all. Every relocation in the span points into
// OVERLAY 2 BSS, so the ROM bytes are twelve ov002 DS addresses and on the host
// each of those symbols is a separate array at an unrelated address. Emitted by
// romdata.py the loop would hand dExtSimpleModel_c::LoadFile twelve DS addresses; that is
// why the name is not on its NAMED list and must not be put there.
//
// hal/level_boot.cpp's port_stage_preload_shared_models once spelled these
// twelve by name, in this order, for exactly this reason -- the port's
// hand-rolled stand-in for the ROM's loop, written when the table itself could
// not be mounted. It now indexes this table at every Stage boot and its release
// twin at every teardown (run hunt2, lane GAMEOVER1), as the ROM's two loops
// do. This row is the ROM's own twelve. The order is the ROM's, read out of the arm9 relocation
// table and quoted per slot below; it is not this file's choice.
//
// ptr_audit STAYS AT ZERO through this change: the audit's subject is
// romdata.py's NAMED list, and this name is not on it before or after.
extern unsigned char data_ov002_0210da48[], data_ov002_0210d9b8[],
    data_ov002_0210da50[], data_ov002_0210d9f8[], data_ov002_0210da40[],
    data_ov002_0210d9a0[], data_ov002_0210d9c0[], data_ov002_0210e7d8[],
    data_ov002_0210e3a0[], data_ov002_0211094c[], data_ov002_0211095c[],
    data_ov002_0210d9a8[];

/* from:0x020756f0 to:0x0210da48 | 0x020756f4 to:0x0210d9b8
   from:0x020756f8 to:0x0210da50 | 0x020756fc to:0x0210d9f8
   from:0x02075700 to:0x0210da40 | 0x02075704 to:0x0210d9a0
   from:0x02075708 to:0x0210d9c0 | 0x0207570c to:0x0210e7d8
   from:0x02075710 to:0x0210e3a0 | 0x02075714 to:0x0211094c
   from:0x02075718 to:0x0211095c | 0x0207571c to:0x0210d9a8
   all kind:load module:overlay(2) */
void *data_020756f0[12] = {
    data_ov002_0210da48, data_ov002_0210d9b8, data_ov002_0210da50,
    data_ov002_0210d9f8, data_ov002_0210da40, data_ov002_0210d9a0,
    data_ov002_0210d9c0, data_ov002_0210e7d8, data_ov002_0210e3a0,
    data_ov002_0211094c, data_ov002_0211095c, data_ov002_0210d9a8,
};
DSSTATE_END

/* The FATAL lines below name each slot by its own DS address (table base +
   4 * slot; the bases are config/arm9/symbols.txt's) or by its roster index.
   Until catchup1 lane ROMDATA they printed each slot's ROM WORD instead, out of
   three tables that spelled those words; the words are relocs.txt's targets
   and a reader finds them there, so the port carries no copy. */
static const unsigned kKuppaScriptTableDs = 0x020876e4;   /* sDanceCutsceneTable */
static const unsigned kSharedModelTableDs = 0x020756f0;   /* data_020756f0 */

/* Called once before the first frame, the sibling of
   port_particle_vtables_check(). It catches the two ways this file can rot: a
   slot left null (something else seated the symbol and this file lost the
   tie-break) and a slot holding a word in the DS address range, which is what
   a raw ROM word looks like and is the whole bug class. Cheap, and it says so
   on frame 0 instead of ten thousand frames into a play session. */
static int port_ptr_slot_bad(const void *p)
{
    const unsigned v = (unsigned)(size_t)p;
    return !v || (v >= 0x01ff0000u && v < 0x02400000u);
}

void port_ptr_tables_check(void)
{
    for (int i = 0; i < 9; ++i)
        if (port_ptr_slot_bad((const void *)fsi_default_command[i])) {
            std::fprintf(stderr, "FATAL: card command table word %08x holds "
                         "%08x, not a host pointer\n", 0x02086758u + i * 4,
                         (unsigned)(size_t)fsi_default_command[i]);
            std::abort();
        }
    for (int i = 0; i < 39; ++i)
        if (port_ptr_slot_bad(port_cam_script_records[i][0])) {
            std::fprintf(stderr, "FATAL: camera script record "
                         "port_cam_script_records[%d] (command %d) holds "
                         "%08x, not a host pointer\n", i, i,
                         (unsigned)(size_t)port_cam_script_records[i][0]);
            std::abort();
        }
    for (int i = 0; i < 6; ++i)
        if (port_ptr_slot_bad(sDanceCutsceneTable[i])) {
            std::fprintf(stderr, "FATAL: kuppa script table slot %d (ROM "
                         "word at %08x) holds %08x, not a host pointer\n", i,
                         kKuppaScriptTableDs + 4u * (unsigned)i,
                         (unsigned)(size_t)sDanceCutsceneTable[i]);
            std::abort();
        }
    for (int i = 0; i < 12; ++i)
        if (port_ptr_slot_bad(data_020756f0[i])) {
            std::fprintf(stderr, "FATAL: shared-model table slot %d (ROM "
                         "word at %08x) holds %08x, not a host pointer\n", i,
                         kSharedModelTableDs + 4u * (unsigned)i,
                         (unsigned)(size_t)data_020756f0[i]);
            std::abort();
        }
}

/* ---- table 3: data_0209a03c, one word, and it is the same bug -------------
 *
 * config/arm9/relocs.txt has exactly three rows for this address:
 *
 *     from:0x0201a4b4 kind:load to:0x0209a03c    src/func_0201a490.c reads it
 *     from:0x020417c4 kind:load to:0x0209a03c    src/func_0204175c.c reads it
 *     from:0x0209a03c kind:load to:0x02057e30    THE WORD ITSELF
 *
 * The third row is the one that matters: the word AT 0x0209a03c is relocated,
 * so the four bytes romdata.py would hand over are the DS address 0x02057e30
 * and calling through them on the host is a jump into nothing. Both readers
 * CALL it -- src/func_0201a490.c is one line, `data_0209a03c(data_0208ee60)`
 * -- which is why hal/boot_os.cpp refused func_0201a490 outright rather than
 * hosting the word: "hosting the raw word would call into nothing".
 *
 * 0x02057e30 is src/func_02057e30.c, and the whole ROM body is `{}`. So the
 * binding is the ordinary one this file makes everywhere above: the host
 * pointer to the host function, not the DS address of the DS function. The
 * cast is because the ROM's call site spells the pointer `void (*)(void *)`
 * while the callee is decompiled `void (void)` -- the DS passes the argument
 * in r0 and the empty body never reads it, and cdecl lets the caller clean up,
 * so the one-argument shape is the faithful spelling of the call.
 *
 * It is .data, not .bss, so it goes inside the DSSTATE bracket with the tables
 * above and the save state rolls it back like the rest. */
extern void func_02057e30(void);
DSSTATE_BEGIN
void (*data_0209a03c)(void *) = (void (*)(void *))func_02057e30;
DSSTATE_END

}  /* extern "C" */

/* src/func_02008b4c.c, reached from commands 12 and 38 through func_02009414,
   calls the spline evaluator by its bare address name. The config knows it as
   _Z19func_ov002_020effb8P7Vector3iS0_S0_S0_S0_ -- 0x020effb8 is inside the Player overlay -- and
   src/func_ov002_020effb8.c is the same matched function. A weak alias ties
   the two spellings together without a forwarding thunk; it only binds if
   nothing else defines the bare name. */
