/* PORT_HOST_ABI.  dScMgTrampoline_c's name-spelling repairs and its one hosted
 * arm9 BSS global.  Run mg11, lane TTI.
 *
 * Nothing here is a decompilation and nothing here changes behaviour.  Every
 * row is a C-named ROM symbol that some TU in this class's closure declares at
 * C++ LINKAGE, so MSVC mangles the reference and the plain C definition the
 * mount already provides does not satisfy it.  link.exe prints the hint itself
 * on each one ("symbols that are defined and could potentially match:
 * _data_ov006_...").  port/tools/alternatename_guard.py is the check that a row
 * is doing something rather than being shadowed by a real definition, and it
 * runs post-link on every map.
 *
 * A DUPLICATE ROW IS HARMLESS.  An /alternatename with the same left and right
 * sides declared twice is deduplicated by the linker, so a sibling lane that
 * reaches the same body will find these standing and needs no coordination.
 *
 * ---- 1. THE THIRTEEN SharedFilePtr ROWS ----------------------------------
 *
 * src/Trmp3DMario_Destruct.cpp is the ELEMENT DESTRUCTOR of the four
 * dMgTrmpln3DMario_c records at this+0x500c (port/slice_tti.txt section 8).
 * It is a //cpp TU that declares thirteen of ov006's SharedFilePtr .bss
 * objects as `extern SharedFilePtr data_ov006_...;`, so each mangles as
 * ?data_ov006_...@@3USharedFilePtr@@A.  The mount defines all thirteen as
 * plain C names.
 *
 * ---- 2. THE SEVENTEEN int AND short ROWS OF THE FIRST WAVE ---------------
 *
 * Same TU and its neighbours, same shape, different types: FOURTEEN `int`
 * (?...@@3HA) and THREE `short` (?...@@3FA), which is seventeen.  The heading
 * on this section used to say SIXTEEN while the sentence under it said
 * fourteen and three -- a heading and its own body disagreeing by one, which
 * is what a summary written before the list it summarises does.
 *
 * THE WHOLE FILE'S CENSUS, so a reader does not have to add up four waves.
 * Counted off the committed file, 59 /alternatename rows:
 *
 *     13  ?...@@3USharedFilePtr@@A     section 1
 *     25  ?...@@3HA          int       sections 2, 2b, 2c, 2d
 *      4  ?...@@3PAHA        int*      sections 2c, 2d
 *      3  ?...@@3FA          short     section 2
 *      3  ?...@@3UG2@@A      struct G2 section 2b
 *      3  ?...@@3PAXA        void*     sections 2, 2c
 *      1  ?...@@3US8@@A      struct S8 section 2c
 *      1  ?...@@3PAPAXA      void**    section 2c
 *      1  ?...@@3EA          uchar     section 2
 *      5  the bare names and the two C++ spellings, sections 3 and 5
 *
 * sTrmpStartTouchX/48/4c are the three halfwords chain link 0
 * (src/minigames/d_s_mg_trampoline.cpp) and the state setter src/minigames/d_s_mg_trampoline.cpp
 * seed the object's +0x5db0..+0x5db6 block from.
 *
 * TWO OF THE int ROWS ARE VTABLES AND NOT DATA, which is worth naming rather
 * than leaving in a list of sixteen: ?_ZTV18dMgTrmpln3DMario_c@@3HA is
 * dMgTrmpln3DMario_c's own five-slot table -- the ELEMENT VTABLE this seat
 * fills -- and ?_ZTV19dMgTrmpObjAdapter_c@@3HA is the neighbouring class's, which the
 * same element destructor also names.
 *
 * ---- 3. THREE BARE NAMES THAT ARE NOT THE SAME SYMBOL AT ALL -------------
 *
 * These three are stronger than a linkage mismatch: the src spells a name that
 * NO config symbol carries, and the ROM address behind it belongs to something
 * with a different name.  Each was resolved from the literal pool rather than
 * from the spelling.
 *
 *   _ZTV18dMgTrmpln3DMario_c   src/Trmp3DMario_Construct.c's element constructor
 *       writes it into word 0 of each record.  The pool word at 0x020cd154
 *       reads 0x0213b2c4 (config: `from:0x020cd154 kind:load to:0x0213b2c4`),
 *       so the vtable IS _ZTV18dMgTrmpln3DMario_c and the name is the class's, taken
 *       from the ROM's own type_info: 0x0213b2c0 -> 0x0213b244 +4 -> 0x0213b270
 *       = "18dMgTrmpln3DMario_c", whose base 0x0213b19c names
 *       "19dMgTrmpObjAdapter_c".
 *
 *   func_020beb74   src/minigames/d_s_mg_d3d_base.cpp uses it as `extern int
 *       func_020beb74[]` and writes func_020beb74[1] and gGraphCallback.  It is
 *       not a function: config/arm9/overlays/ov004/symbols.txt has
 *       `sMgGraphCallback kind:bss addr:0x020beb74`.  The src's `func_`
 *       prefix is a decompiler default applied to an address it could not
 *       classify, and the alias points it at the mount's bss object.
 *
 *   Scene_AfterRender   src/minigames/d_s_mg_d3d_base.cpp tail-
 *       calls it.  The ROM body 0x020e700c is `ldr ip,[pc,#4] / bx ip` and its
 *       one pool word at 0x020e703c reads 0x0202e398 (config:
 *       `from:0x020e703c kind:load to:0x0202e398 module:main`), which
 *       config/arm9/symbols.txt names `_ZN8dScene_c11AfterRenderEj`.  So the alias
 *       is a plain C name onto the mangled ARM one, which is the opposite
 *       direction from every other row in this file and is why it is spelled
 *       out.
 *
 * ---- 4. ONE HOSTED arm9 BSS GLOBAL ---------------------------------------
 *
 * data_020a0dbc is arm9 .bss (config/arm9/symbols.txt, `kind:bss`) and nothing
 * in the port hosted it before this seat.  src/minigames/d_s_mg_trampoline.cpp (vtable
 * slot 18) reads it as `volatile short data_020a0dbc[]` at indices 0 and 1 to
 * seed the object's +0x5db0..+0x5db6 stylus block.
 *
 * IT IS SIZED BY ROM SPAN AND NOT BY FIELD WIDTH, which is the rule an
 * undersized host global broke once already: the next arm9 symbol is
 * @20, so the ROM span is FOUR bytes and the two shorts the source
 * reads fill it exactly.  It is declared as eight ints to match every other row
 * in hal/auto_bss.cpp, whose header calls the sizes generous defaults -- the
 * hazard that rule guards against is a host object SHORTER than the ROM span,
 * and this is longer.  It lives inside the DSSTATE bracket so the save state
 * captures it, the same as auto_bss.cpp's rows; it is here rather than there so
 * three sibling lanes editing the same shared file in one run do not collide.
 */

#include "hal/dsstate_seg.h"

/* THE DEFINITION IS NOT HERE ANY MORE, AND THE PARAGRAPH ABOVE IS WHY.

   This lane declared `int data_020a0dbc[8]` -- 32 bytes -- reasoning that a
   host object LONGER than the ROM span is the safe direction and that matching
   hal/auto_bss.cpp's generous-default rows was the tidier choice.  Lane TTE
   hosted the same symbol in hal/auto_bss.cpp as `short data_020a0dbc[2]`, and
   the mg11 merge found the two colliding.  TTE's is correct and this one was
   not, on four independent readings:

     src/minigames/d_s_mg_trampoline2.cpp:34  extern volatile s16 data_020a0dbc[];
                                     reads [0] and [1] with ldrsh
     src/__sinit_02075054.c:2        extern short data_020a0dbc[];
                                     writes [0] and [1], and passes
                                     @20 as a SEPARATE symbol
     config/arm9/symbols.txt:4928    data_020a0dbc at 0x020a0dbc; the NEXT
                                     symbol is @20 -- a 4-byte span
     the paragraph above                'the ROM span is FOUR bytes and the two
                                     shorts the source reads fill it exactly'

   The fourth reading is this lane's own, one line above the declaration it
   contradicted.  Eight ints do not merely over-reserve: they cover
   @20 and the seven symbols after it, and any TU that took ITS
   indexing from this declaration would read [1] at +4 where the ROM's ldrsh
   reads +2.  No TU did -- every reader carries its own s16 declaration -- so
   this was a latent span defect rather than a live misread, which is exactly
   the class the sized-by-ROM-span rule exists to catch.

   Save-state capture is unaffected: auto_bss.cpp's rows sit inside its own
   DSSTATE bracket (hal/auto_bss.cpp:12..419), so the symbol is still
   captured.  Section 4 above is kept for its derivation of WHY the port needs
   the symbol at all. */

/* ---- 1. the thirteen SharedFilePtr rows ---------------------------------- */
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
#pragma comment(linker, "/alternatename:?sTrmpMarioRollJumpEndAnimFile@@3USharedFilePtr@@A=_sTrmpMarioRollJumpEndAnimFile")

/* ---- 2. the int and short rows ------------------------------------------ */
#pragma comment(linker, "/alternatename:?sTrmpAnimStruggle@@3HA=_sTrmpAnimStruggle")
#pragma comment(linker, "/alternatename:?sTrmpAnimRollJumpEnd@@3HA=_sTrmpAnimRollJumpEnd")
#pragma comment(linker, "/alternatename:?sTrmpAnimDoubleJump1@@3HA=_sTrmpAnimDoubleJump1")
#pragma comment(linker, "/alternatename:?sTrmpAnimWalk@@3HA=_sTrmpAnimWalk")
#pragma comment(linker, "/alternatename:?sTrmpAnimTurnEnd@@3HA=_sTrmpAnimTurnEnd")
#pragma comment(linker, "/alternatename:?sTrmpAnimTurn@@3HA=_sTrmpAnimTurn")
#pragma comment(linker, "/alternatename:?sTrmpAnimJumpEnd@@3HA=_sTrmpAnimJumpEnd")
#pragma comment(linker, "/alternatename:?sTrmpAnimWallJump@@3HA=_sTrmpAnimWallJump")
#pragma comment(linker, "/alternatename:?sTrmpAnimJump@@3HA=_sTrmpAnimJump")
#pragma comment(linker, "/alternatename:?sTrmpAnimBounce@@3HA=_sTrmpAnimBounce")
#pragma comment(linker, "/alternatename:?sTrmpAnimDoubleJump2@@3HA=_sTrmpAnimDoubleJump2")
#pragma comment(linker, "/alternatename:?sTrmpAnimHangUp@@3HA=_sTrmpAnimHangUp")
/* the two that are VTABLES rather than data -- section 2 */
#pragma comment(linker, "/alternatename:?_ZTV18dMgTrmpln3DMario_c@@3HA=__ZTV18dMgTrmpln3DMario_c")
#pragma comment(linker, "/alternatename:?_ZTV19dMgTrmpObjAdapter_c@@3HA=__ZTV19dMgTrmpObjAdapter_c")
/* the three halfwords the stylus block is seeded from */
#pragma comment(linker, "/alternatename:?sTrmpStartTouchX@@3FA=_sTrmpStartTouchX")
#pragma comment(linker, "/alternatename:?sTrmpStartTouchY@@3FA=_sTrmpStartTouchY")
#pragma comment(linker, "/alternatename:?sTrmpIntroFromX@@3FA=_sTrmpIntroFromX")
/* two arm9 globals the middle base's own bodies declare at C++ linkage */
#pragma comment(linker, "/alternatename:?data_0209f5f8@@3EA=_data_0209f5f8")
#pragma comment(linker, "/alternatename:?sD3DSubObjPalette@@3PAXA=_sD3DSubObjPalette")

/* ---- 2b. the SECOND wave's rows, same shapes -----------------------------
   Two more ints and three `struct G2` objects, all from the element-object
   machinery the first wave pulled in (_ZN18dMgTrmpln3DMario_c6RenderEv, _020cb030,
   _020ccd78, _020cf758).  sTrmpAnimWalk appears TWICE in this file, once
   as ?...@@3HA and once as ?...@@3UG2@@A: two TUs declare the same mount word
   with two different C++ types, so each mangling needs its own row and neither
   is redundant. */
#pragma comment(linker, "/alternatename:?sTrmpMariosInPlay@@3HA=_sTrmpMariosInPlay")
#pragma comment(linker, "/alternatename:?sTrmpRowHeights@@3HA=_sTrmpRowHeights")
#pragma comment(linker, "/alternatename:?pmf_Trmp3DMario_StateWalkIn@@3UG2@@A=_pmf_Trmp3DMario_StateWalkIn")
#pragma comment(linker, "/alternatename:?pmf_Trmp3DMario_StateOnGround@@3UG2@@A=_pmf_Trmp3DMario_StateOnGround")
#pragma comment(linker, "/alternatename:?sTrmpAnimWalk@@3UG2@@A=_sTrmpAnimWalk")

/* ---- 2d. the FOURTH wave, all the same shape ------------------------- */
#pragma comment(linker, "/alternatename:?pmf_Trmp2Mario_StateAppearWait_2@@3PAHA=_pmf_Trmp2Mario_StateAppearWait_2")
#pragma comment(linker, "/alternatename:?pmf_Trmp3DMario_StateBounce_2@@3PAHA=_pmf_Trmp3DMario_StateBounce_2")
#pragma comment(linker, "/alternatename:?pmf_Trmp2Mario_StateBounce_2@@3PAHA=_pmf_Trmp2Mario_StateBounce_2")
#pragma comment(linker, "/alternatename:?sTrmp3DGravityStep@@3HA=_sTrmp3DGravityStep")
#pragma comment(linker, "/alternatename:?sTrmp3DJumpOffSpeed@@3HA=_sTrmp3DJumpOffSpeed")
#pragma comment(linker, "/alternatename:?sTrmp3DBounceScale@@3HA=_sTrmp3DBounceScale")
#pragma comment(linker, "/alternatename:?sTrmp2GravityStep@@3HA=_sTrmp2GravityStep")
#pragma comment(linker, "/alternatename:?sTrmpAnimBounce@@3PAXA=_sTrmpAnimBounce")

/* ---- 2c. the THIRD wave. Same shapes; two of them are a SECOND mangling
   of a word this file already carries (sTrmpRowHeights as int and as
   int*, sTrmpBounceAnimTable as void**), which is two TUs disagreeing about
   the type of one mount word rather than a duplicate row. */
#pragma comment(linker, "/alternatename:?sTrmpRowHeights@@3PAHA=_sTrmpRowHeights")
#pragma comment(linker, "/alternatename:?sTrmp2Lives@@3HA=_sTrmp2Lives")
#pragma comment(linker, "/alternatename:?sTrmpSpeedScale@@3HA=_sTrmpSpeedScale")
#pragma comment(linker, "/alternatename:?data_ov006_02140598@@3HA=_data_ov006_02140598")
#pragma comment(linker, "/alternatename:?sTrmp2RiseSpeedMax@@3HA=_sTrmp2RiseSpeedMax")
#pragma comment(linker, "/alternatename:?sTrmp2FallSpeed@@3HA=_sTrmp2FallSpeed")
#pragma comment(linker, "/alternatename:?pmf_Trmp2Mario_StateWallBounce@@3US8@@A=_pmf_Trmp2Mario_StateWallBounce")
#pragma comment(linker, "/alternatename:?sTrmpBounceAnimTable@@3PAPAXA=_sTrmpBounceAnimTable")
#pragma comment(linker, "/alternatename:?sMgMainCamera@@3PAXA=_sMgMainCamera")

/* ---- 3. the three bare names, resolved from their literal pools ---------- */
/* catchup1 Stage C: the 0x0213b2c4 row is retired. TARGET names that vtable
   _ZTV18dMgTrmpln3DMario_c and the ov006 mount defines that name itself, so
   the address-keyed rename had left this row aliasing the name to itself. */
#pragma comment(linker, "/alternatename:_func_020beb74=_sMgGraphCallback")
#pragma comment(linker, "/alternatename:_Scene_AfterRender=__ZN8dScene_c11AfterRenderEj")
/* ---- 5. RETIRED at catchup1 (the main -> port sync, Stage B) ---------------
   This section held the void* dExtAnmModel_c::SetAnim shim and the
   LoadTextureToVram alias.  Their callers (src/Trmp3DMario_EnterState1.cpp,
   _020cb690.cpp, src/TrmpLine_LoadTextures.cpp) are folded into main's real
   minigame TUs, which call the real members through include/dExtAnmModel_c.h
   and include/dExtSimpleModel_c.h, so nothing references either old
   spelling.  Both rows are deleted. */

/* src/minigames/d_mg_jump3d_mario.cpp calls func_ov006_020e6df0, which is not a
   symbol: config/arm9/overlays/ov006/symbols.txt names 0x020e6df0
   _Z21Sound_PlayBank1Pannediii, and delinks.txt gives it its own .text block
   0x020e6df0..0x020e6e3c.  Same bare-name shape as section 3. */
#pragma comment(linker, "/alternatename:_func_ov006_020e6df0=__Z21Sound_PlayBank1Pannediii")

/* MSVC emits nothing for a TU that is only pragmas, and an empty object still
   carries the linker directives. This symbol exists so a reader grepping for
   who owns these rows finds a definition rather than only a comment. */
extern "C" const char port_mg_tti_faces_marker[] =
    "dScMgTrampoline_c faces, run mg11 lane TTI";
