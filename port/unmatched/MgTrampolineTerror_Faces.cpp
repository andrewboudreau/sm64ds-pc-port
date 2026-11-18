// PORT_HOST_ABI.  dScMgTrampoline2_c's closure faces.  Run mg11, lane TTE.
// actor id 0x181 = scene 385, "Trampoline Terror".
//
// Four kinds of row live here and they are kept apart on purpose, because only
// one of the four is a pure linker rename and the other three change or assert
// something about the CALL:
//
//   1. MSVC-TYPED SPELLINGS of mounted C storage -- 67 /alternatename rows.
//      A matched .cpp TU declares a data symbol outside its extern "C" block,
//      so MSVC mangles the reference while port/tools/ovdata.py's mount defines
//      the one real C name.  Data only; a data alias has no this-register
//      contract to break.  This is hal/actor_classes_bbh.cpp's IDENTITY_MATRIX4X3
//      case, 67 words deep.
//   2. FOUR FUNCTION ALIASES, every one of them cdecl on BOTH sides.  The
//      hal/actor_classes_ov063.cpp law is that an /alternatename can never
//      bridge cdecl into __thiscall; it says nothing about cdecl to cdecl,
//      where the stack layout and the return register are identical and there
//      is no `this` to place.  Each is argued on its own line below.
//   3. RETIRED at catchup1 (the main -> port sync, Stage B): the three method faces.
//   4. FOUR FLOOR TRAPS: ov006 bodies on this class's live path with no src
//      file and no delinks block.  Count and return, never invent.
//
// EVERY ROW HERE WAS ASKED FOR BY A LINK.  The closure was NOT walked from the
// ROM's relocation graph -- port/slice_lky.txt section 9 records why that
// over-reaches -- it was closed the documented way: seed with the class's own
// TUs, link, add exactly what LNK2019 names, repeat.

// ---- 1. MSVC-TYPED SPELLINGS OF MOUNTED C STORAGE -------------------------
#pragma comment(linker, "/alternatename:?data_0209f5f8@@3EA=_data_0209f5f8")
#pragma comment(linker, "/alternatename:?data_020a0dbc@@3RCFC=_data_020a0dbc")
#pragma comment(linker, "/alternatename:?pmf_Trmp2Mario_StateHidden@@3PAHA=_pmf_Trmp2Mario_StateHidden")
#pragma comment(linker, "/alternatename:?_ZTV17dMgTrmpln2Mario_c@@3HA=__ZTV17dMgTrmpln2Mario_c")
#pragma comment(linker, "/alternatename:?sTrmpItemTextureAnim@@3UBTA_File@@A=_sTrmpItemTextureAnim")
#pragma comment(linker, "/alternatename:?_ZTV19dMgTrmpObjAdapter_c@@3HA=__ZTV19dMgTrmpObjAdapter_c")
#pragma comment(linker, "/alternatename:?sTrmpAnimStruggle@@3HA=_sTrmpAnimStruggle")
#pragma comment(linker, "/alternatename:?sTrmpAnimRollJumpEnd@@3HA=_sTrmpAnimRollJumpEnd")
#pragma comment(linker, "/alternatename:?sTrmp2Marios@@3PADA=_sTrmp2Marios")
#pragma comment(linker, "/alternatename:?sTrmpAnimDoubleJump1@@3HA=_sTrmpAnimDoubleJump1")
#pragma comment(linker, "/alternatename:?sTrmpAnimWalk@@3HA=_sTrmpAnimWalk")
#pragma comment(linker, "/alternatename:?sTrmpAnimTurnEnd@@3HA=_sTrmpAnimTurnEnd")
#pragma comment(linker, "/alternatename:?sTrmpAnimTurn@@3HA=_sTrmpAnimTurn")
#pragma comment(linker, "/alternatename:?sTrmpAnimJumpEnd@@3HA=_sTrmpAnimJumpEnd")
#pragma comment(linker, "/alternatename:?sTrmpAnimWallJump@@3HA=_sTrmpAnimWallJump")
#pragma comment(linker, "/alternatename:?sTrmpAnimJump@@3HA=_sTrmpAnimJump")
#pragma comment(linker, "/alternatename:?sTrmpAnimBounce@@3HA=_sTrmpAnimBounce")
#pragma comment(linker, "/alternatename:?sTrmpAnimDoubleJump2@@3HA=_sTrmpAnimDoubleJump2")
#pragma comment(linker, "/alternatename:?sTrmp2DriftScale@@3HA=_sTrmp2DriftScale")
#pragma comment(linker, "/alternatename:?sTrmp2MarioTarget@@3HA=_sTrmp2MarioTarget")
#pragma comment(linker, "/alternatename:?sTrmp2MarioCount@@3HA=_sTrmp2MarioCount")
#pragma comment(linker, "/alternatename:?sTrmpAnimHangUp@@3HA=_sTrmpAnimHangUp")
#pragma comment(linker, "/alternatename:?sTrmpAnimSdown@@3HA=_sTrmpAnimSdown")
#pragma comment(linker, "/alternatename:?sTrmpMarioDoubleJump1AnimFile@@3USharedFilePtr@@A=_sTrmpMarioDoubleJump1AnimFile")
#pragma comment(linker, "/alternatename:?sTrmpMarioRollJumpAnimFile@@3USharedFilePtr@@A=_sTrmpMarioRollJumpAnimFile")
#pragma comment(linker, "/alternatename:?sTrmpMarioTurnAnimFile@@3USharedFilePtr@@A=_sTrmpMarioTurnAnimFile")
#pragma comment(linker, "/alternatename:?sTrmpMarioDoubleJump2AnimFile@@3USharedFilePtr@@A=_sTrmpMarioDoubleJump2AnimFile")
#pragma comment(linker, "/alternatename:?sTrmpMarioWalkAnimFile@@3USharedFilePtr@@A=_sTrmpMarioWalkAnimFile")
#pragma comment(linker, "/alternatename:?sTrmpMarioModelFile@@3USharedFilePtr@@A=_sTrmpMarioModelFile")
#pragma comment(linker, "/alternatename:?sTrmpMarioJumpEndAnimFile@@3USharedFilePtr@@A=_sTrmpMarioJumpEndAnimFile")
#pragma comment(linker, "/alternatename:?sTrmpMarioJumpAnimFile@@3USharedFilePtr@@A=_sTrmpMarioJumpAnimFile")
#pragma comment(linker, "/alternatename:?sTrmpMarioTurnEndAnimFile@@3USharedFilePtr@@A=_sTrmpMarioTurnEndAnimFile")
#pragma comment(linker, "/alternatename:?sTrmpMarioHangUpAnimFile@@3USharedFilePtr@@A=_sTrmpMarioHangUpAnimFile")
#pragma comment(linker, "/alternatename:?sTrmpMarioStruggleAnimFile@@3USharedFilePtr@@A=_sTrmpMarioStruggleAnimFile")
#pragma comment(linker, "/alternatename:?sTrmpMarioWallJumpAnimFile@@3USharedFilePtr@@A=_sTrmpMarioWallJumpAnimFile")
#pragma comment(linker, "/alternatename:?sTrmpMarioSdownAnimFile@@3USharedFilePtr@@A=_sTrmpMarioSdownAnimFile")
#pragma comment(linker, "/alternatename:?sTrmpMarioRollJumpEndAnimFile@@3USharedFilePtr@@A=_sTrmpMarioRollJumpEndAnimFile")
#pragma comment(linker, "/alternatename:?sTrmpItemRingCModelFile@@3Udata_t@@A=_sTrmpItemRingCModelFile")
#pragma comment(linker, "/alternatename:?sTrmpItemRingAModelFile@@3Udata_t@@A=_sTrmpItemRingAModelFile")
#pragma comment(linker, "/alternatename:?sTrmpItemRingBModelFile@@3Udata_t@@A=_sTrmpItemRingBModelFile")
#pragma comment(linker, "/alternatename:?sD3DSubObjPalette@@3PAXA=_sD3DSubObjPalette")
#pragma comment(linker, "/alternatename:?sTrmp2StarModelFile@@3USharedFilePtr@@A=_sTrmp2StarModelFile")
#pragma comment(linker, "/alternatename:?pmf_TrmpItem_StateAppear_2@@3US2@@A=_pmf_TrmpItem_StateAppear_2")
#pragma comment(linker, "/alternatename:?sTrmpItemsActive@@3HA=_sTrmpItemsActive")
#pragma comment(linker, "/alternatename:?sTrmpItemCount@@3HA=_sTrmpItemCount")
#pragma comment(linker, "/alternatename:?sTrmp2Lives@@3HA=_sTrmp2Lives")
#pragma comment(linker, "/alternatename:?pmf_Trmp3DMario_StateWalkIn@@3UG2@@A=_pmf_Trmp3DMario_StateWalkIn")
#pragma comment(linker, "/alternatename:?pmf_Trmp2Mario_StateWallBounce@@3US8@@A=_pmf_Trmp2Mario_StateWallBounce")
#pragma comment(linker, "/alternatename:?pmf_Trmp3DMario_StateOnGround@@3UG2@@A=_pmf_Trmp3DMario_StateOnGround")
#pragma comment(linker, "/alternatename:?pmf_Trmp2Mario_StateAppearWait_2@@3PAHA=_pmf_Trmp2Mario_StateAppearWait_2")
#pragma comment(linker, "/alternatename:?pmf_Trmp3DMario_StateBounce_2@@3PAHA=_pmf_Trmp3DMario_StateBounce_2")
#pragma comment(linker, "/alternatename:?pmf_Trmp2Mario_StateBounce_2@@3PAHA=_pmf_Trmp2Mario_StateBounce_2")
#pragma comment(linker, "/alternatename:?sTrmpBounceAnimTable@@3PAPAXA=_sTrmpBounceAnimTable")
#pragma comment(linker, "/alternatename:?sTrmp3DGravityStep@@3HA=_sTrmp3DGravityStep")
#pragma comment(linker, "/alternatename:?sTrmp3DJumpOffSpeed@@3HA=_sTrmp3DJumpOffSpeed")
#pragma comment(linker, "/alternatename:?sTrmpAnimWalk@@3UG2@@A=_sTrmpAnimWalk")
#pragma comment(linker, "/alternatename:?sTrmpSpeedScale@@3HA=_sTrmpSpeedScale")
#pragma comment(linker, "/alternatename:?sTrmp3DBounceScale@@3HA=_sTrmp3DBounceScale")
#pragma comment(linker, "/alternatename:?sTrmpMariosInPlay@@3HA=_sTrmpMariosInPlay")
#pragma comment(linker, "/alternatename:?data_ov006_02140598@@3HA=_data_ov006_02140598")
#pragma comment(linker, "/alternatename:?sTrmpAnimBounce@@3PAXA=_sTrmpAnimBounce")
#pragma comment(linker, "/alternatename:?sTrmp2RiseSpeedMax@@3HA=_sTrmp2RiseSpeedMax")
#pragma comment(linker, "/alternatename:?sTrmp2GravityStep@@3HA=_sTrmp2GravityStep")
#pragma comment(linker, "/alternatename:?sTrmp2FallSpeed@@3HA=_sTrmp2FallSpeed")
#pragma comment(linker, "/alternatename:?sTrmpRowHeights@@3HA=_sTrmpRowHeights")
#pragma comment(linker, "/alternatename:?sTrmpRowHeights@@3PAHA=_sTrmpRowHeights")
#pragma comment(linker, "/alternatename:?sMgMainCamera@@3PAXA=_sMgMainCamera")

// ---- 2. FOUR FUNCTION ALIASES, ALL cdecl ON BOTH SIDES --------------------
//
// RETIRED at catchup1 (the main -> port sync, Stage B): the LoadTextureToVram row.  Its caller
// src/TrmpLine_LoadTextures.cpp is folded into src/minigames/unit020cd744.cpp,
// which calls the real static dExtSimpleModel_c::LoadTextureToVram, so the
// namespace-scope spelling the alias bridged is referenced by nothing.
//
// Particle::System::New.  src/Trmp2Mario_Update.cpp declares a local
// `struct System { static System* New(...); }` with seven parameters, so MSVC
// wants ?New@System@@SAPAU1@IIHHHPBUVector3_16f@@PAUCallback@@@Z -- SA, a
// static member, __cdecl.  The matched body is
// src/_ZN8Particle6System3NewEjj5Fix12IiES2_S2_PK11Vector3_16fPNS_8CallbackE.c,
// a .c file, so it is defined at C linkage with the same seven arguments in the
// same order.  Static member and C function are the same call surface on x86:
// seven stack arguments, caller cleans, pointer back in eax.  That TU is
// already linked by port/slice_gate29.txt, so this row only renames.
#pragma comment(linker, "/alternatename:?New@System@@SAPAU1@IIHHHPBUVector3_16f@@PAUCallback@@@Z=__ZN8Particle6System3NewEjjiiiPK11Vector3_16fPN5dPa_c7level_c10callback_cE")
//
// Scene::AfterRender.  include/decl_common.h:1936 declares
// `extern void Scene_AfterRender(void*, unsigned int);` while the matched body
// is src/_ZN8dScene_c11AfterRenderEj.cpp, already linked by port/slice_w1l2.txt
// under the ROM's own Itanium name (config/arm9/symbols.txt:1105,
// _ZN8dScene_c11AfterRenderEj at 0x0202e398 -- the address dScMgD3DBase_c's slot
// 11 tail-jumps to, which is how this lane found it).  Both are C linkage and
// both take (void*, unsigned): the receiver is an explicit first parameter on
// both sides, so this is a rename and not a receiver placement.
#pragma comment(linker, "/alternatename:_Scene_AfterRender=__ZN8dScene_c11AfterRenderEj")
//
// AND ONE THAT IS NOT EVEN A TYPE RENAME.  src/minigames/d_s_mg_d3d_base.cpp declares
// `extern int func_020beb74[];` and USES IT AS DATA
// (`func_020beb74[1] = (int)obj; gGraphCallback = (void *)func_020beb74;`).
// There is no function at 0x020beb74: config/arm9/overlays/ov004/symbols.txt
// has sMgGraphCallback, kind bss, and 0x020beb74 is inside ov004's bss span
// 0x020beb60..0x020bfec0.  A func_-prefixed name on a data address is a
// decomp-side naming defect; the port renames rather than editing the
// byte-gated tree.  ROUTED to the decomp side as a symbol-name correction.
#pragma comment(linker, "/alternatename:_func_020beb74=_sMgGraphCallback")
//
// ApproachLinear2, the INT overload.  src/_Z15ApproachLinear2Riii.cpp defines
// `int ApproachLinear2(int &, int, int)` with NO extern "C", so MSVC decorates
// it ?ApproachLinear2@@YAHAAHHH@Z, while three .c TUs in this closure
// (src/TrmpItem_GrowScale.c, _020cf790.c and _020cf820.c) spell the ROM's
// Itanium name at C linkage.  port/unmatched/Ov085_Behaviors.cpp:142 records
// the same fact and solves it the other way, by declaring the C++ signature in
// a C++ TU; a .c caller cannot do that.  Both sides are plain __cdecl free
// functions taking (int&, int, int) and returning int in eax, so the alias has
// no receiver contract to break -- the same shape as the retired
// LoadTextureToVram row, with a reference instead of a pointer.
#pragma comment(linker, "/alternatename:__Z15ApproachLinear2Riii=?ApproachLinear2@@YAHAAHHH@Z")
//
// _Z21Sound_PlayBank1Pannediii.  src/Trmp2Mario_BounceOffLineA.cpp and
// src/minigames/d_mg_jump3d_mario.cpp call `func_ov006_020e6df0(a, b, c)` by address-
// derived name, while config/arm9/overlays/ov006/symbols.txt:690 gives 0x020e6df0
// the recovered name _Z21Sound_PlayBank1Pannediii (size 0x4c) and the matched TU is
// src/minigames/d_s_mg_d3d_base.cpp, a slice line here.  Same address, same three
// C-linkage arguments; the row is a rename and nothing else.
#pragma comment(linker, "/alternatename:_func_ov006_020e6df0=__Z21Sound_PlayBank1Pannediii")

// ---- 3. RETIRED at catchup1 (the main -> port sync, Stage B) ------------------
//
// This section held three cdecl-into-__thiscall method faces: daPly_c's
// St_Null_Init, dExtAnmTexSRT_c::SetFile and a note on the void* SetAnim.
// Every caller was a shadow-class TU (src/TrmpItem_Load.cpp,
// src/Trmp2Mario_EnterState1.cpp, src/Trmp3DMario_PickIdleAnim.cpp) that main
// folded into real TUs calling the real members through their headers, and
// src/minigames/d_s_mg_trampoline2.cpp calls Trmp2Mario_UpdateAll() directly.
// The faces are deleted.

// ---- 4. FOUR FLOOR TRAPS --------------------------------------------------
//
// Three ov006 bodies on this class's live path have NO src file and NO delinks
// block in config/arm9/overlays/ov006/delinks.txt.  They are trapped count-and-
// return, never invented, and their want counts are printed by
// hal/scene_mg_trampoline2.cpp's census.  A trap-shaped floor also HIDES its
// callees from static closure, so retiring one of these costs its own slice
// lines on top of the body.
//
//   _ZN8TrmpLine4DrawEv  0x45c  void(char*)                 src/TrmpLine_DrawAll.c
//   _ZN8TrmpLine5BuildEPsS0_  0x800  void(short*,short*,short*)  src/TrmpLine_CreateStartLineT2.c
//   _ZN8TrmpLine6CreateEPsS0_  0x3ac  int(u16*,u16*)              src/minigames/d_s_mg_trampoline2.cpp
//                                                           (THIS class's slot 23)
//   _ZN8TrmpLine10PushJumperEv  0x56c  void(char*)                 src/TrmpLine_StateFlex.c
//
// THE THIRD ONE IS A PLAYER-FACING GAP AND IT IS NAMED AS ONE.  Slot 23
// (OnAttacked2) packs the current and previous stylus points into four
// halfwords and asks _ZN8TrmpLine6CreateEPsS0_ whether the stroke connected; the true
// arm plays func_02012718(0x1af, ...) and the false arm plays
// func_02012790(0xe).  A trap that returns 0 takes the false arm every time, so
// on the port every stroke reads as a miss at that test.  The trap says so on
// its first call rather than reading as a quiet correct answer.  Its arity and
// return type are taken from the two CALL SITES that name it
// (src/minigames/d_s_mg_trampoline.cpp and src/minigames/d_s_mg_trampoline2.cpp), which agree.
//
// SECTION 4 IS HISTORY NOW.  All four of those bodies are seated -- the first
// three by runs mg12/mg13 in unmatched/MgTrampolineTime_Floors.cpp's slice, and
// the fourth, _ZN8TrmpLine10PushJumperEv, by run mg13 lane BOUNCE.  This file traps
// nothing.  The paragraph above is kept because it is the derivation of the
// arities the seats use, not because any of it is still a floor.

#include <cstdio>
#include <cstdlib>

/* THE FIRST THREE TRAPS MOVED AT THE MERGE.  Lane TTI trapped the same three
   bodies in unmatched/MgTrampolineTime_Floors.cpp -- 0x180 and 0x181 share
   this machinery -- and the two copies collided at link (LNK2005 x3).  ONE
   definition each survives, in TTI's file, and it uses THIS lane's signatures:
   both lanes ruled all three UNDECOMPILED with the same sizes, and the only
   difference was that TTI spelled the first two `int f(void)` as placeholders
   while these were derived from the ROM call sites that name them.  The counts
   below still come out of the same trap bodies; port_mg_shared_trap_counts is
   TTI's file handing them back, and the census this file feeds is unchanged.

   The FOURTH trap stays here: _ZN8TrmpLine10PushJumperEv is behind one of this
   class's 3D-Mario states and only 0x181 reaches it. */


/* THIS FILE'S OWN TRAP IS RETIRED.  It used to define _ZN8TrmpLine10PushJumperEv
   count-and-return, 0x56c, with no src and no delinks block, and run mg12 lane
   TRM had already read the listing and called it the bounce.  Run mg13 lane
   BOUNCE decompiled it: src/TrmpLine_PushJumper.c is a slice_tte.txt line now,
   an honest NONMATCHING seat at 8 divergences against 339 ROM code words with
   the size, the 8-word literal pool and all 28 call offsets exactly the ROM's,
   and with every one of those 8 divergences a permutation or a re-association
   of the same computation (its banner enumerates them).  It needs no hostgen:
   the body has no MMIO store and its pool holds no 0x04xxxxxx word.

   WHAT THAT CHANGES FOR A READER OF THE CENSUS.  This body is what a trampoline
   RECORD's per-tick state runs to decide whether any of the up-to-five
   characters in sTrmpJumpers crossed the trampoline this frame.  While
   it was trapped it was reached constantly and did nothing, so the trampolines
   drew (run mg13 lane TRAMP seated the mesh) and every character fell straight
   through them -- which is exactly what Tango reported on the 9c4517c2b build.
   The observable that moves is in port/hal/trampoline_records.cpp's readout:
   this body DECREMENTS the record's remaining count at +0x324 on each catch and
   bumps the caught count at +0x326, and sets the render gate at +0x328 to 3 when
   the count reaches zero.  With the trap in place +0x324 could only ever hold
   the value the installer left there. */
static const unsigned g_tte_trap_020cfc74 = 0;   /* RETIRED, see above */

/* THE CHARACTER CENSUS, added with the seat by run mg13 lane BOUNCE.
   src/TrmpLine_PushJumper.c walks sTrmpJumpers -- the up-to-five
   character records Trmp2Mario_InitPool (stride 0xdc) and Trmp3DMario_InitPool
   (stride 0xd0) register -- and writes each caught character's bounce state to
   its +0x18.  Without this readout a run in which nothing bounces cannot be told
   apart from a run in which nothing was ever REGISTERED, and those two want
   opposite follow-ups.  It reads and prints; it writes nothing.  It hangs off
   port_mg_tte_trap_counts because that is this file's own function and the
   census in hal/scene_mg_trampoline2.cpp already calls it once at the end of a
   run -- no other lane's file is touched to get it. */
extern "C" {
extern unsigned char *sTrmpJumpers[];
extern unsigned char sTrmpLines[];      /* the four records, stride 0x32c */
}

/* THE GEOMETRY THE TEST ACTUALLY READS.  _ZN8TrmpLine10PushJumperEv projects each
   character onto the record's two axes at +0x14 and +0x20, about the origin at
   +0x08, and tests against the segment that runs +/- the half-length at +0x58
   along the first axis.  Those four fields are written by the installer
   _ZN8TrmpLine5BuildEPsS0_, not by the bounce, so printing them separates "the test
   ran and nothing crossed" from "the test ran against a segment of length
   zero".  Plain memory reads of a symbol the ov006 mount already defines: no
   vtable is called and nothing is written. */
static void port_mg_tte_record_geometry(void)
{
    int i;
    for (i = 0; i < 4; ++i) {
        const unsigned char *r = &sTrmpLines[i * 0x32c];
        const int *o = (const int *)(r + 0x08);
        const int *u = (const int *)(r + 0x14);
        const int *v = (const int *)(r + 0x20);
        std::printf("[scene] trampoline record %d geometry: origin +0x08 = "
                    "(%d, %d, %d), axis +0x14 = (%d, %d, %d), axis +0x20 = "
                    "(%d, %d, %d), half-length +0x58 = %d, gate +0x328 = %u\n",
                    i, o[0], o[1], o[2], u[0], u[1], u[2], v[0], v[1], v[2],
                    *(const int *)(r + 0x58), (unsigned)r[0x328]);
    }
}

static void port_mg_tte_character_census(void)
{
    int i;
    int present = 0;
    int stated = 0;

    port_mg_tte_record_geometry();

    for (i = 0; i < 5; ++i) {
        const unsigned char *o = sTrmpJumpers[i];
        if (o == 0) {
            std::printf("[scene] dScMgTrampoline2_c character slot %d: ABSENT "
                        "(sTrmpJumpers[%d] == 0)\n", i, i);
            continue;
        }
        ++present;
        {
            const unsigned st = *(const unsigned short *)(o + 0x18);
            const int px = *(const int *)(o + 0x04);
            const int py = *(const int *)(o + 0x08);
            const int pz = *(const int *)(o + 0x0c);
            if (st != 0) ++stated;
            std::printf("[scene] dScMgTrampoline2_c character slot %d: at %p, "
                        "bounce state +0x18 = %u, axis +0x04 = "
                        "(%d, %d, %d), +0x10 = %d, +0x14 = %d\n",
                        i, (const void *)o, st, px, py, pz,
                        *(const int *)(o + 0x10), *(const int *)(o + 0x14));
            /* RAW WORDS, so a run in which nothing crossed can be read against
               the record geometry above without calling a virtual. The record's
               segment sits at y = -720896 (1.12, so -176.0); a character whose
               words never reach that band never had a crossing to find. Read
               only; 0xdc is the element stride Trmp2Mario_InitPool registers.
               Behind SM64DS_TTE_CHARDUMP=1 because it is 55 words per character
               and this census prints on every scene-385 run. */
            if (std::getenv("SM64DS_TTE_CHARDUMP")) {
                int k;
                std::printf("[scene] dScMgTrampoline2_c character slot %d raw:",
                            i);
                for (k = 0; k < 0xdc; k += 4)
                    std::printf(" +%02x=%d", k, *(const int *)(o + k));
                std::printf("\n");
            }
        }
    }
    std::printf("[scene] dScMgTrampoline2_c characters: %d of 5 registered, %d "
                "carrying a nonzero bounce state RIGHT NOW. DO NOT READ A 0 HERE "
                "AS 'NOTHING WAS CAUGHT': +0x18 is a state the character's own "
                "state machine consumes within the frame, so a run that caught "
                "prints 0 of 5 here whenever the census lands after the bounce "
                "finished. WHAT SURVIVES A CATCH is the record's live count at "
                "+0x324 and its caught count at +0x326 in the record readout "
                "above, and the caught character's own +0x04..+0x0c axis with "
                "+0x10 and +0x14 in the per-slot lines -- those are written by "
                "the hit tail of _ZN8TrmpLine10PushJumperEv and nothing else clears "
                "them. Read those three, not this one\n", present, stated);
}

extern "C" void port_mg_tte_trap_counts(unsigned *a, unsigned *b, unsigned *c,
                                        unsigned *d)
{
    /* the first three come out of the unified trap bodies in
       unmatched/MgTrampolineTime_Floors.cpp; the fourth was this file's own and
       is now a retirement, not a measurement -- it can only read 0 */
    if (a) *a = 0;      /* the three shared trap bodies are all decompiled and seated */
    if (b) *b = 0;
    if (c) *c = 0;
    if (d) *d = g_tte_trap_020cfc74;
    port_mg_tte_character_census();
}

/* This symbol exists so a reader grepping for who owns the aliases above finds
   a definition rather than only a comment. */
extern "C" const char port_mg_tte_faces_marker[] =
    "dScMgTrampoline2_c faces, run mg11 lane TTE";
