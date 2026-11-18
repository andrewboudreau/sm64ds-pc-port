/* ---------------------------------------------------------------------------
 * ctor_runner.cpp -- THE ROM'S OWN STATIC-INITIALISER RUNNER, AT ENTRY'S POINT.
 *
 * Run link100, lane CTOR, rung C1a. Lane CWSCOUT measured the root; this file
 * lands it.
 *
 * WHAT THE ROM DOES. Entry (arm9 0x02004800) ends with three instructions and
 * a tail jump, read out of extracted/arm9_dec.bin at base 0x02004000:
 *
 *     020048a0  bl 0x1ffafd4
 *     020048a4  bl 0x2019780        NitroStartUp()
 *     020048a8  bl 0x2072f94        __call_static_initializers()      <-- THIS ONE
 *     020048ac  ldr r1, [pc, #0x1c]     -> 0x02007000 = main
 *     020048b0  ldr lr, [pc, #0x1c]     -> 0xffff0000
 *     020048b4  bx  r1
 *
 * __call_static_initializers is CodeWarrior's __init_cpp / static-initialiser runner and
 * src/func_02072f94.c is the whole of it:
 *
 *     fn_t *p = (fn_t *)0x2086b60;
 *     while (p != 0 && (f = *p) != 0) { f(); p++; }
 *
 * 0x02086b60 is the .ctor table: 23 __sinit_* addresses and a NULL terminator
 * at 0x02086bbc, confirmed both ways -- config/arm9/symbols.txt:3210-3232 names
 * the 23 words .p__sinit_<addr>, and the image itself reads:
 *
 *     02086b60: 02073a24   02086b7c: 02074e0c   02086b98: 02074fe4
 *     02086b64: 02073e6c   02086b80: 02074e44   02086b9c: 0207501c
 *     02086b68: 02074d90   02086b84: 02074e80   02086ba0: 02075054
 *     02086b6c: 02074da8   02086b88: 02074e84   02086ba4: 020750b4
 *     02086b70: 02074dbc   02086b8c: 02074edc   02086ba8: 020750b8
 *     02086b74: 02074dc0   02086b90: 02074f80   02086bac: 020750ec
 *     02086b78: 02074dc4   02086b94: 02074fb8   02086bb0: 0207511c
 *                                               02086bb4: 02075150
 *                                               02086bb8: 02075154
 *                                               02086bbc: 00000000
 *
 * WHERE THE TABLE LIVES IN THIS PORT, and why it is not just a host array.
 * The runner is a MATCHED TU and it reads a LITERAL DS ADDRESS. The port's
 * hosted DS globals are ordinary host symbols wherever the linker puts them,
 * so a host array called data_02086b60 would be invisible to that load. What
 * IS at 0x02086b60 in this process is DS main RAM: ntr/io.cpp reserves
 * 0x02000000..0x02400000 zero-filled from a TLS callback before the CRT runs,
 * which is the same mapping hal/level_boot.cpp leans on when it says a raw DS
 * pointer read "does not fault, because ntr/io.cpp reserves the DS address
 * space zero-filled". So the table is PUBLISHED there: kCtorTable below is the
 * ROM's 23 words plus the terminator with every word BOUND to the host body
 * that answers for that initialiser, and port_rom_entry_ctors() copies it to
 * 0x02086b60 one instruction before the ROM's own runner walks it.
 *
 * That is the port's romdata pointer-table contract, applied to a table the
 * port had never mounted: a relocated word carries a DS address, and a DS
 * address the port cannot execute must be bound to the host body before
 * anything dispatches through it. 0x02086b60 is arm9 .rodata (the ROM's own
 * .data does not start until 0x02086bc0), nothing else in this process writes
 * that page, and no overlay mounts anywhere near it -- the level and minigame
 * overlays start at 0x0210xxxx.
 *
 * MAIN RAM IS THE ONE RESERVED RANGE ntr/io.cpp CALLS SURVIVABLE TO LOSE, so
 * the publish is checked rather than assumed. If the page is not committed and
 * writable the runner is NOT called (a walk from an unmapped literal is an
 * access violation with no message), the table is run directly instead, and
 * the [ctor] line says so. That branch is a host safety net for a mapping the
 * port already documents as raceable; it is not the normal path and it says
 * when it is taken.
 *
 * WHERE IT IS CALLED FROM. hal/rom_main.cpp's port_rom_main_run(), at its
 * head, which tests/walk_window.cpp calls immediately after
 * port_boot_rom_pre_main() and Heap::InitializeRootHeap() -- NitroStartUp's
 * own body and its fourth call. So the port's order is now Entry's order:
 * NitroStartUp, then __call_static_initializers, then main. Nothing was moved to make room.
 *
 * WHAT RUNS AND WHAT DOES NOT. After rungs C1a, C1b, C1d, C1e, C1f and C1g,
 * and after run link100 lane CTOR3 took C1c out and put it back on a seat that
 * holds, TWENTY-ONE of the 23 words are bound to the ROM's own initialiser and
 * TWO are bound to a FACE that names the initialiser it stands for and
 * returns. The faces do not
 * abort. Word 0 was a face until rung C1e and an abort there would have killed
 * every boot before the first frame, which is where the rule came from; it
 * still holds for the five that are left, because a face that aborts turns a
 * named gap into a dead build and tells nobody anything the message did not
 * already say. They report once each, by name, and the
 * summary line counts them, which is what makes the walk complete AND honest:
 * the runner really visits all 23 words, and the run says exactly which ones
 * the port cannot answer for yet.
 *
 * THE TWO HAND CALLS ARE GONE IN THIS COMMIT. __sinit_KuppaScript.cpp was called from
 * hal/level_boot.cpp's port_intro_seat_scripts() and __sinit_d_wipe.cpp from a
 * C++ static object in hal/fdr_arm9_fader_seat.cpp. Both are in the link
 * already (slice_intro.txt and slice_fdr.txt) and both are now words in this
 * table, so leaving the hand calls would have run them TWICE. Neither is
 * idempotent in the interesting direction -- __sinit_d_wipe.cpp pushes a
 * destructor node onto __global_destructor_chain through __register_global_object, so a second run
 * pushes a second node for the same object -- which is why the two words go
 * through counting shims and the [ctor] line prints both counters. One each is
 * the pass; anything else is a defect this file will say out loud.
 *
 * ORDER, stated because it is the whole risk of these rungs:
 *   * The eight initialisers rung C1a adds allocate nothing. Five have empty
 *     bodies; __sinit_d_actor.cpp zeroes two words of data_0209b468;
 *     __sinit_Sound.cpp calls _Z13func_02011a5cPv, which zeroes data_0209b53c;
 *     __sinit_d_s_mb.cpp stores two pointers into sMBGraphCallback. No call to
 *     Heap::, Memory::, operator new or malloc appears anywhere in the eight
 *     or in _Z13func_02011a5cPv, so none of them can touch a game heap that Entry
 *     has not built yet. (The root heap DOES exist here -- walk_window calls
 *     Heap::InitializeRootHeap one line above -- and the game heap does not;
 *     neither is needed.)
 *   * All three globals are zeroed host BSS at this point in the boot and no
 *     host seat writes any of them before Entry: data_0209b468 and
 *     data_0209b53c are hal/actor_vtables.cpp storage that only actor and
 *     sound code fills, later; sMBGraphCallback is hal/scene_link100_boot.cpp
 *     storage whose only ROM reader (_ZN7dScMB_c13InitResourcesEv) is not in this link.
 *   * The ov002 static initialisers tests/walk_window.cpp calls by hand keep
 *     their place AFTER this call, unchanged. That is the ROM's order too:
 *     ov002 is an overlay and is not resident at Entry, so its .ctor work
 *     cannot precede the arm9 table's. Nothing in the eight names an ov002
 *     symbol.
 *   * hal/level_boot.cpp still seats sCutsceneEndingCakeForMario's one code pointer at intro
 *     time. __sinit_KuppaScript.cpp writes bytes 0x22..0x25, 0x3e..0x41 and
 *     0x30d..0x310 of that blob and the seat writes the word at 0x180, so the
 *     two are disjoint and the order between them cannot change a byte.
 *
 * RUNG C1b ADDS SIX MORE, and they allocate nothing either: five of the six are
 * a handful of field stores plus one __register_global_object push, and the sixth
 * (__sinit_Timer.cpp) additionally calls func_0203b9b4 on data_0209d4b8 and
 * Timer's constructor _Z13func_0201964cP5Timer on gRaceTimer, both of which are field
 * stores into storage hal/auto_bss.cpp already hosts. The same grep over the
 * six and over _Z13func_0201964cP5Timer returns nothing for Heap, Memory, operator new or
 * malloc.
 *
 * TWO OF C1b's NINE ARE DELIBERATELY LEFT FACED and the reasons are in their
 * face messages:
 *   __sinit_func_0201a96c.cpp calls func_0201aa18, whose chain is
 *   func_0201aa18 -> func_0201aad4 -> func_0201aac8 -> OS_CreateAlarm, and
 *   src/func_0201aac8.c is a tail-call veneer decompiled as `void(void)` that
 *   its caller invokes WITH an argument. On ARM the value rides r0 through the
 *   veneer; on the host it does not. Linking that at Entry would hand
 *   OS_CreateAlarm whatever the host left in a register. That is a PORT_HOST_ABI
 *   question, not a linkage one.
 *   __sinit_d_bg_w_kc_mbg.cpp CONSTRUCTS a RaycastLine into data_020a0d0c, and
 *   hal/clsn_vtable.cpp hosts that as a 0x10 grouped-section block whose
 *   neighbours hal/mmc_vtable.cpp:148 asserts at ROM spacing. The constructor
 *   writes past +0x64. Whether that stays inside the band is a layout question
 *   owned by those two files.
 *
 * RUNG C1d ADDS __sinit_d_s_stage_methods.cpp, the arm9 Clipper's own initialiser. It is
 * four statements: two pointer stores into sStageGraphCallback (the second wins; the
 * first is dead in the ROM itself), Clipper's constructor on data_0209f43c, and
 * one __register_global_object push. It allocates nothing -- _ZN7ClipperC1Ev writes its
 * vptr and calls Clipper::Func_020156DC, which is four field stores and a call
 * to Func_0201559C, and that one is fixed-point arithmetic over the object's
 * own words. The same grep for Heap, Memory, operator new and malloc over the
 * three bodies returns nothing.
 *
 * WHAT C1d HAD TO HOST, and why each one was safe to host here:
 *   _ZTV7Clipper   0x0208e730..0x0208e738, TWO words, both relocated
 *                  (config/arm9/relocs.txt 0x0208e730 -> 0x02015720 = D1,
 *                  0x0208e734 -> 0x020156fc = D0). Nothing in the port hosted
 *                  it and both readers -- src/_ZN7ClipperC1Ev.cpp and
 *                  src/_ZN7ClipperD1Ev.cpp -- are this rung's own TUs. Word 0 is
 *                  bound to the ROM's D1 body. WORD 1 IS A NAMED TRAP, not a
 *                  body: src/_ZN7ClipperD0Ev.cpp calls `base_dtor_Clipper`, an
 *                  INVENTED name for 0x0203cbcc that nothing in this tree
 *                  defines, so the deleting destructor is not linkable today.
 *                  It cannot fire either -- the only Clipper in this image is
 *                  the static data_0209f43c and nothing deletes it -- and it
 *                  says so out loud if it ever does, the shape
 *                  hal/arm9_tables_link100.cpp's a9t_trap rows take.
 *   data_0209f388  0x0209f388..0x0209f394, 12 bytes, the destruct-node cell of
 *                  this initialiser's __register_global_object push. Hosted nowhere. It
 *                  falls inside the nominal span of hal/auto_bss.cpp's generic
 *                  `int gStarMarkerTypes[8]`, the same slack the C1b cells sit
 *                  beside: gStarMarkerTypes's own ROM extent is 12 bytes and its
 *                  three readers (_Z13SetStarMarkeriP8dActor_ci, Minimap::Render,
 *                  Minimap::Behavior) index it as a byte array [0..7], so no
 *                  reader crosses +12 and the generosity is slack rather than
 *                  aliasing.
 *
 * AND FIVE MSVC NAMES, because src/__sinit_02074e84.cpp is a //cpp TU that
 * declares its five data names WITHOUT extern "C". Every one of the five LHS
 * strings below was transcribed from `dumpbin /symbols` on this tree's own
 * compile of that TU (tmp/probe), the rule hal/w8a_stage_storage.cpp's alias
 * block sets out: a hand-built mangling that is one letter off is a directive
 * that never fires and never says so.
 *
 * WHAT C1d WRITES IS DISPATCHED ON EVERY LEVEL FRAME NOW (run linkfull, lane
 * K7GC2). sStageGraphCallback's first word becomes &_ZTVN10dScStage_c15graphCallback_cE (the Stage's
 * graph-callback table, hal/arm9_tables_link100.cpp section 3, all four slots
 * seated and the table registered with the frame beat). Its readers go through
 * gGraphCallback, which hal/level_boot.cpp points at sStageGraphCallback on every Stage
 * build, at Stage::InitResources:284's point, and clears at
 * Stage::CleanupResources:77's: the beat at _Z13func_02019144v's head calls slot 2,
 * Stage::GraphCallback2 (the minimap's BG3 affine), and frame phase 2
 * (_Z13func_02019390v) calls slot 0. When this rung landed nothing seated that
 * pointer, so the vptr stored here had no reader yet. data_0209f43c gets the
 * ROM's own 4:3 seed (0x1555, 0xe38, 0x1000, 0x01388000) at Entry, which
 * Camera::Render re-seeds through the same Clipper::Func_020156DC on every
 * frame anyway -- so the seed is the ROM's own value at the ROM's own point,
 * not a new one.
 *
 * RUNG C1e ADDS __sinit_d_camera.cpp, WORD 0 of the table and the last of the
 * campaign's clean rungs. It is 38 struct copies: the ROM's 19 camera State
 * objects at 0x0209b008..0x0209b138, step 0x10, each {onEnter PMF, main PMF},
 * filled from 38 relocated eight-byte pairs in arm9 .rodata at
 * 0x02086bc8..0x02086e48. It calls nothing and allocates nothing.
 *
 * THE DESTINATIONS ARE hal/camera_states.cpp's AND THEY STAY ITS. That file
 * hosts all nineteen at their ROM names and BAKES THE SAME VALUES IN, and says
 * so in its own header: "That sinit is not run on host (its 38 source pairs are
 * ROM pointer data that would each need its own symbol); the objects are
 * synthesized here with the same addresses, read out of the arm9 image once and
 * baked in below." This rung supplies exactly the thing that sentence says was
 * missing -- the 38 symbols -- and runs the ROM's own copy instead.
 *
 * SO THE WRITE IS PROVED REDUNDANT BEFORE IT IS ADDED, not assumed to be. All
 * 19 x 4 words were read out of extracted/arm9_dec.bin through the sinit's own
 * source-to-destination mapping and compared against camera_states.cpp's
 * nineteen CAMSTATE lines: 19 of 19 identical, both function words and both
 * zero this-deltas. The full table is lane CTOR2's report. The bake-in
 * is compile-time initialised data, so the values are already right before
 * Entry and the ROM's initialiser then writes the same bytes over them -- an
 * idempotent second write, not a second address for the same bytes and not a
 * host seat that decides the outcome. hal/camera_states.cpp is NOT this lane's
 * file: its bake-in and the two sentences above it are now redundant rather
 * than wrong, and retiring them is a proposal in this lane's report, not an
 * edit here.
 *
 * THE 38 KEEP THEIR DS ADDRESSES, and that is required rather than tolerated.
 * They are mwcc pointers to member -- {code address, this-delta} -- and nothing
 * calls through them: _ZN9dCamera_c10EnterStateEv and _ZN9dCamera_c11UpdateStateEv read the word and hand it
 * to hal/camera_states.cpp's hal_call_camera_state_fn, which translates the DS
 * address to a host call through camera_states.inc. Binding these words to host
 * bodies the way data_0208ee14 below is bound would break that translation.
 * That is the port's standing mwcc pointer-to-member exception, at the storage
 * this time rather than at a call.
 *
 * Every word below was read twice and the two had to agree: the raw image at
 * extracted/arm9_dec.bin base 0x02004000, and config/arm9/relocs.txt, which
 * carries a `kind:load` row at each pair's FIRST word and none at its second --
 * so the code half is a relocation and the this-delta half is a literal zero,
 * in all 38.
 *
 * RUNG C1c IS BACK, AND WHAT IT TOOK IS IN hal/scene_boot.cpp.
 * __sinit_d_scene.cpp constructs the two arm9 fader statics -- a FaderColor into
 * gSceneColorFader and a FaderBrightness into gSoftResetBrightnessFader, three vptr stores
 * each in base-to-derived order (data_0208eafc = Fader, data_0208eacc =
 * FaderBrightness, and for the first one data_0208eb2c = FaderColor), two
 * field stores each, and one __register_global_object push each. All three tables are
 * hosted by hal/scene_boot.cpp. The only names it wanted are the two 12-byte
 * destruct-node cells, which nothing hosted; they are below.
 *
 * IT IS THE ONE RUNG OF THIS CAMPAIGN THAT CHANGES WHAT A LIVE OBJECT
 * DISPATCHES, and that is exactly what went wrong the first time.
 * hal/fader_wipes.cpp placement-news a host HalFaderWipe over gSceneColorFader
 * before main, so once this word is bound to the ROM's body the ROM's
 * initialiser is the LAST writer of that vptr and the installed colour fader
 * dispatches hal/scene_boot.cpp's data_0208eb2c instead of the host class's
 * table.
 *
 * THE FIRST LANDING (lane CTOR2, 6dae1a0bd) FOUND THAT TABLE EMPTY.
 * hal/scene_boot.cpp filled data_0208eb2c from scene_fill_title(), one row of
 * port_scene_registry_install()'s walk, which is far later in the boot than
 * Entry -- so between the two every one of its ten words was zero. Run link100
 * lane WM9's loopback pair died in both windows, rc=0xC0000005, at
 * _ZN10dScEntry_c10StartMatchEv -> LoadLevelNoReturn -> _Z9LoadLevelijija ->
 * Scene::SetAndStopColorFader -> Scene::SetFaders+0x27, whose faulting frame
 * is EIP 0 with eax = data_0208eb2c and ecx = gSceneColorFader, both read out of
 * that build's own walk_window.map. Scene::SetFaders asks the INSTALLED fader
 * `vt->f14` -- ROM byte +0x14, FaderBrightness::IsAtStart -- before replacing
 * it. The single-player gates never see it: the title comes up first there, so
 * the fill has already run by the time any level loads, which is why a tail2
 * 4000-frame run and the whole battery read green over a pair-killing bug.
 * Lane CTOR3's rung 0 (043f4b5ce) took the word back to its face on that
 * evidence.
 *
 * WHAT MAKES THE RE-LAND HONEST is that the seat now runs BEFORE main.
 * hal/scene_boot.cpp's l2_seat_fader_vtables() is called from a C++ static
 * initialiser, so all thirty-four words of data_0208ea6c, data_0208eb2c and
 * data_0208eacc are live before port_rom_main_run() walks the ROM's .ctor
 * table -- the same order the cartridge has, where those tables are constants
 * in the image from the first instruction. scene_fill_title() still calls the
 * same function and it is idempotent, so the title path did not move. The
 * census below reads that file's own two counters and prints them next to the
 * vptrs, so "the table was live when the initialiser installed it" is a
 * measurement in every captured run rather than a claim in a comment.
 *
 * WHAT IT RETIRES. hal/method_faces.cpp's FaderBrightness block said in as
 * many words that gSoftResetBrightnessFader's vptr is null "because the port does not link
 * the ROM's static initialiser for it (src/__sinit_02074edc.c)", that the
 * matched SetForwardTime ends in an unqualified virtual IsAtEnd() that would
 * fault reading [vptr+0x14] on a null table, and "Do not wake the branch
 * without giving gSoftResetBrightnessFader a real vptr first." This rung is that. The vptr
 * is now data_0208eacc, whose slot 5 is hal/scene_boot.cpp's named l2_vt_trap
 * rather than address zero, so the worst case moved from an access violation
 * to a line of output. The branch itself was unreachable for its own separate
 * reason -- every call site is behind `gSoftResetPending != 0` and that byte's
 * only writer, src/func_02023498.c, was not in the link. It is now (run
 * linkfull, lane LOOPIN2: phase 0x17 of both host loops), so the soft-reset
 * combo on a level enters the branch for two frames -- SetForwardTime on this
 * object, then IsAtEnd -- before the level loop answers the reset by starting
 * the game again at the title (hal/method_faces.cpp's face banner and
 * tests/walk_window.cpp's phase-0x17 block carry the account). This object's
 * constructed vptr is what makes those two calls land on bodies rather than
 * on address zero.
 *
 * hal/fader_wipes.cpp is NOT this lane's file in the sense that its
 * placement-new is now the pre-Entry value rather than the final one. Lane
 * CTOR3 measured whether that construction should go at all and the answer is
 * in this lane's report: it must stay, because gSceneFader is pre-seated to
 * hal_wipes[0] and six other wipes share the class.
 *
 * RUNGS C1f AND C1g ARE THE TWO WORDS LANE CTOR REFUSED THAT RE-MEASURED AS
 * RUNGS. Both refusals rested on a reading of another file's hosting, and both
 * readings were checked again against this tree's own map and source.
 *
 * C1f, __sinit_d_bg_w_kc_mbg.cpp. CTOR refused it because RaycastLine's constructor
 * writes past +0x64 of data_020a0d0c and that symbol's host block is 0x10.
 * THE BLOCK IS 0x78. hal/clsn_vtable.cpp hosts the whole run contiguously in
 * ordered sections at ROM spacing -- data_020a0d0c 0x10, data_020a0d1c 0x44,
 * data_020a0d60 0x24, ending at 0x020a0d84 which is the next dsd symbol -- and
 * hal/mmc_vtable.cpp:148 asserts both deltas at run time on every boot,
 * precisely so a linker that stopped packing them would say so. +0x64 is 0x14
 * bytes inside that band. The three moving-collider DetectClsn bodies already
 * fill the same object every time they run, so a construction at Entry is the
 * ROM's own first fill and not a new writer. Both bodies it names were already
 * in the link; the only name it wanted is the destruct-node cell
 * @260, below. Nothing in the constructor's chain (_ZN5dBgChC2Ev,
 * _ZN5dBgPiC2Ev, _ZN8dM3dGSphC1Ev) names Heap, Memory, operator new or malloc.
 *
 * C1g, __sinit_Vec3_ApproachHorz.cpp. CTOR called it a CONFLICT because hal/auto_bss.cpp's
 * generic `int data_020a0db0[8]` is 32 bytes and covers data_020a0db8,
 * @20 and @19. IT IS SLACK, and that is measured rather
 * than argued: data_020a0db0's own ROM extent is 4 bytes and every reader in
 * the tree treats it as ONE WORD -- a bitmask tested with &1, &8, &0x10 and
 * &0x18 in MgBounceAndPounce::BeforeBehavior, HUD::RenderStarCount,
 * Stage::RenderBouncingArrows and Message::Update -- so nothing crosses +4 and
 * a byte-exact host beside it is the same shape as every other cell in this
 * file. The fourth name, data_020a0dbc, is ALREADY hosted byte-exact by
 * hal/auto_bss.cpp at its 4-byte ROM span, and that is the one the three ROM
 * readers use, so the initialiser writes the same storage they read.
 *
 * ONE DEFECT IS NAMED AND NOT FIXED HERE, because it is older than this rung
 * and it is another file's: the ROM bytes at 0x020a0dbc already have TWO host
 * addresses today, hal/auto_bss.cpp's byte-exact short data_020a0dbc[2] and
 * the interior of its own int data_020a0db0[8]. Linking this initialiser does
 * not add to that and does not depend on it -- it writes the byte-exact
 * symbol -- but narrowing data_020a0db0 to its ROM span is a one-line fix that
 * file should take whether or not this word is ever linked.
 *
 * ONE ROM DATA OBJECT IS HOSTED HERE, because __sinit_d_s_mb.cpp names it and
 * nothing else in the port does: data_0208ee14, 16 bytes at arm9 .data, the
 * four-slot Scene graph-callback table. Its words are code addresses and
 * config/arm9/relocs.txt carries the relocations, so it gets the same bind
 * treatment as the .ctor table itself. Scene::GraphCallback0/1/3 are already
 * in the link; GraphCallback2 joins them because this table's third word names
 * it. Every one of the four is an eight-byte `return 1`.
 * ---------------------------------------------------------------------------
 */

#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include "dsstate_seg.h"

extern "C" {

/* The runner itself: src/func_02072f94.c, arm9 0x02072f94. */
void __call_static_initializers(void);

/* The initialisers this build answers for. */
/* catchup1 round 3 (lane SLICE4's report): main retired the flat
   __sinit_ pins. Every TU behind them links, and each one either emits its
   initialisers into .CRT$XCU (the CRT runs them once, before main, where the
   DS runs its table) or is constant initialised, or had an empty ROM body.
   So each table word below has nothing left to do on the host and is an
   empty body, kept so the walk stays in ROM order. __sinit_02074e44 was the
   last exception (it stored the RNG seed by hand while func_0201fec8.cpp kept
   `LevelRng data_0209e650;` off the host); that TU now defines and seeds the
   object itself, so it is empty too (catchup1 Stage D). */
void __sinit_02073e6c(void) {}   /* was hand-called from hal/level_boot.cpp */
void __sinit_02073a24(void);     /* C1e: body below, after the pair declarations */
void __sinit_02074d90(void) {}
void __sinit_02074da8(void) {}
void __sinit_02074dbc(void) {}
void __sinit_02074dc0(void) {}
void __sinit_02074dc4(void) {}   /* C1b */
void __sinit_02074e44(void) {}   /* func_0201fec8.cpp seeds data_0209e650 itself */
void __sinit_02074e80(void) {}
void __sinit_02074e84(void) {}   /* C1d */
void __sinit_02074edc(void) {}   /* C1c */
void __sinit_02074fe4(void) {}   /* C1b */
void __sinit_0207501c(void) {}   /* C1f */
void __sinit_02075054(void) {}   /* C1g */
void __sinit_02074f80(void) {}   /* was hand-called from hal/fdr_arm9_fader_seat.cpp */
void __sinit_02074fb8(void) {}
void __sinit_020750b4(void) {}
void __sinit_020750b8(void) {}   /* C1b */
void __sinit_020750ec(void) {}   /* C1b */
void __sinit_0207511c(void) {}   /* C1b */
void __sinit_02075150(void) {}
/* K1LOOP: the five list heads. Main retired the handwritten __sinit_02075154;
   src/fListNode.cpp now builds the five heads with constructors, and MSVC runs
   those five dynamic initialisers from the CRT before main, which is where the
   DS runs __sinit_fListNode.cpp too. The table word therefore has nothing left
   to do on the host: this entry stays (the walk is in ROM order) and is empty. */
void __sinit_02075154(void) {}

/* The two arm9 fader statics rung C1c's initialiser constructs, hosted by
   hal/auto_bss.cpp (gSoftResetBrightnessFader, 12 bytes at the ROM span) and by
   hal/fader_wipes.cpp (gSceneColorFader, the gate-31 placement-new). Read here
   only, to report which vtable each one's vptr word ended up naming. */
extern int gSoftResetBrightnessFader[3];
extern unsigned char gSceneColorFader[];

/* The three fader vtables hal/scene_boot.cpp hosts, for the same report. */
extern void *_ZTV8dFader_c[12];
extern void *_ZTV15dFdBrightness_c[12];
extern void *_ZTV10dFdColor_c[10];
extern void *_ZTV7dWipe_c[12];   /* hal/fdr_arm9_fader_seat.cpp: the ROM-ordered table the scene faders dispatch through */
struct dWipe_c;
extern dWipe_c data_0209f61c;     /* hal/auto_bss.cpp: the minigame scenes' wipe, built by the ROM walk */

/* And that file's own answer about them: whether its seat has run, and how
   many of the thirty-four words it owns are still zero. Rung C1c installs
   data_0208eb2c as a live vptr, so "was the table seated when the initialiser
   installed it" is the question the first landing of that rung got wrong. */
int port_fader_vtable_seated(void);
int port_fader_vtable_zero_words(void);

/* The nineteen camera State objects rung C1e's initialiser fills, hosted by
   hal/camera_states.cpp. Read here and never written: port_rom_entry_ctors
   snapshots their 76 words before the walk and compares them after, so the
   claim that __sinit_d_camera.cpp's fill is byte-for-byte redundant against that
   file's compile-time bake-in is CHECKED on every boot instead of trusted. */
extern unsigned int data_0209b008[4], data_0209b018[4], data_0209b028[4],
                    data_0209b038[4], data_0209b048[4], data_0209b058[4],
                    data_0209b068[4], data_0209b078[4], data_0209b088[4],
                    data_0209b098[4], data_0209b0a8[4], data_0209b0b8[4],
                    data_0209b0c8[4], data_0209b0d8[4], data_0209b0e8[4],
                    data_0209b0f8[4], data_0209b108[4], data_0209b118[4],
                    data_0209b128[4];

/* The MSL global-destructor chain head, hosted in hal/cxx_aliases.cpp. Every
   initialiser that constructs a static pushes a three-word node onto it
   through __register_global_object; nothing ever pops it. Its length is part of the
   [ctor] line because it is the one shape this rung changes that outlives the
   call. */
extern void *__global_destructor_chain;

/* Clipper's destructor, the first word of _ZTV7Clipper below (rung C1d).
   src/_ZN7ClipperD1Ev.cpp, arm9 0x02015720. */
void _ZN8dClipperD1Ev(void *self);

/* Scene's four graph callbacks, the words of data_0208ee14 below. */
int _ZN8dGraph_c10callback_c14GraphCallback0Ev(void);
int _ZN8dGraph_c10callback_c14GraphCallback1Ev(void);
int _ZN8dGraph_c10callback_c14GraphCallback2Ev(void);
int _ZN8dGraph_c10callback_c14GraphCallback3Ev(void);

}  /* extern "C" */

/* ---- data_0208ee14, the four-slot Scene graph-callback table --------------
 *
 * ROM span 0x0208ee14..0x0208ee24 (next symbol in config/arm9/symbols.txt is
 * _ZTSN8dGraph_c10callback_cE), four words, every one of them a relocation into arm9 code:
 *
 *     0208ee14  02018eb8  _ZN8dGraph_c10callback_c14GraphCallback0Ev
 *     0208ee18  02018eb0  _ZN8dGraph_c10callback_c14GraphCallback1Ev
 *     0208ee1c  02018ea8  _ZN8dGraph_c10callback_c14GraphCallback2Ev
 *     0208ee20  02018ea0  _ZN8dGraph_c10callback_c14GraphCallback3Ev
 *
 * It is hosted here and not in hal/arm9_tables_link100.cpp because the reason
 * it is needed is this rung's: src/__sinit_02074fb8.c names it, and that
 * initialiser is one of the eight. The store it appears in is dead in the ROM
 * itself (`sMBGraphCallback[0] = &data_0208ee14;` is overwritten by the next
 * statement with &data_02094390), so nothing in this build reads a word back
 * out of this table -- but a table of code addresses is bound to the bodies
 * whether or not this build dispatches it, because that is the contract and
 * because the next lane to link _ZN7dScMB_c13InitResourcesEv inherits it. */
DSSTATE_BEGIN
extern "C" void *data_0208ee14[4] = {
    (void *)&_ZN8dGraph_c10callback_c14GraphCallback0Ev,
    (void *)&_ZN8dGraph_c10callback_c14GraphCallback1Ev,
    (void *)&_ZN8dGraph_c10callback_c14GraphCallback2Ev,
    (void *)&_ZN8dGraph_c10callback_c14GraphCallback3Ev,
};
DSSTATE_END

/* ---- rung C1b: the seven destruct-node cells --------------------------
 *
 * Every one of them is the third argument of a __register_global_object call in the
 * initialiser that names it -- the three-word node src/func_020731dc.c pushes
 * onto __global_destructor_chain (node[0] = previous head, node[1] = the destructor,
 * node[2] = the object). Nothing in this port hosted any of them, and nothing
 * in this port reads one back: the ROM's only reader of the chain is
 * __destroy_global_chain, the exit-time walker behind MSL's exit(), which is not in the
 * link.
 *
 * SIZED BY ROM SPAN, not by the three words the writer uses, per the port's
 * undersized-globals rule: each is the delta to the next symbol in
 * config/arm9/symbols.txt.
 *
 *     data_0209d4bc  0x0209d4bc..0x0209d4c8   12   __sinit_Timer.cpp
 *     @18  0x0209e654..0x0209e660   12   __sinit_func_0201fec8.cpp
 *     data_020a0ce0  0x020a0ce0..0x020a0cec   12   __sinit_d_bg_w_kc.cpp
 *     @1260  0x020a0eb0..0x020a0ebc   12   __sinit_Heap.cpp
 *     data_020a0ec8  0x020a0ec8..0x020a0ed0    8   __sinit_Vec2_Len.cpp (the object)
 *     @17  0x020a0ed0..0x020a0edc   12   __sinit_Vec2_Len.cpp (the node)
 *     @16  0x020a0ee4..0x020a0ef0   12   __sinit_cstd_sqrt.cpp
 *
 * FOUR OF THE SEVEN ARE NOT COVERED BY ANY OTHER HOST OBJECT AT ALL. The other
 * three (data_0209d4bc, @18, @17) fall inside the nominal
 * span of one of hal/auto_bss.cpp's generic `int[8]` neighbours -- that file's
 * generous default, which it itself narrows to the ROM span wherever a ROM
 * reader cross-reads a block (see its data_020a0dbc note). Nothing cross-reads
 * these: the writer is __register_global_object and the reader is not in the link, so the
 * generosity is slack rather than aliasing and a byte-exact host beside it is
 * the honest shape rather than a second name for live storage. */
DSSTATE_BEGIN
extern "C" {
unsigned char data_0209d4bc[12];
unsigned char data_0209e654[12];
unsigned char data_020a0ce0[12];
unsigned char data_020a0eb0[12];
unsigned char data_020a0ec8[8];
unsigned char data_020a0ed0[12];
unsigned char data_020a0ee4[12];
}
DSSTATE_END

/* ---- rungs C1f and C1g: three more cells and one four-byte word ----------
 *
 *     @260  0x020a0d00..0x020a0d0c  12  C1f, the RaycastLine node
 *     data_020a0db8  0x020a0db8..0x020a0dbc   4  C1g, the two flag halfwords
 *     @20  0x020a0dc0..0x020a0dcc  12  C1g, the first node
 *     @19  0x020a0dcc..0x020a0dd8  12  C1g, the second node
 *
 * All four sized by the delta to the next symbol in config/arm9/symbols.txt
 * and hosted nowhere before this. The header carries why the three that fall
 * inside hal/auto_bss.cpp's int data_020a0db0[8] are slack rather than
 * aliasing. data_020a0db8 is a short pair because that is how
 * src/__sinit_02075054.c writes it. */
DSSTATE_BEGIN
extern "C" {
unsigned char data_020a0d00[12];
short data_020a0db8[2];
unsigned char data_020a0dc0[12];
unsigned char data_020a0dcc[12];
}
DSSTATE_END

/* ---- rung C1c: the two fader destruct-node cells -------------------------
 *
 *     @332  0x0209f5c4..0x0209f5d0  12  the FaderBrightness node
 *     @331  0x0209f5dc..0x0209f5e8  12  the FaderColor node
 *
 * Both sized by the delta to the next symbol in config/arm9/symbols.txt, and
 * hosted nowhere before this. @332 falls inside the nominal span of
 * hal/auto_bss.cpp's generic `int gNextSceneParam[8]`, which is the same slack
 * the C1b and C1d cells sit beside: gNextSceneParam's own ROM extent is 4 bytes
 * and both of its readers (Scene::SetSceneToSpawn writes it, and
 * Scene::SpawnIfNecessary passes it to _Z13func_02013edciii) touch exactly that one
 * word, so nothing crosses +4 and the generosity is slack rather than
 * aliasing. @331 sits between hal/auto_bss.cpp's byte-exact
 * gSoftResetBrightnessFader[0xc] and hal/fader_wipes.cpp's gSceneColorFader and overlaps
 * neither. */
DSSTATE_BEGIN
extern "C" {
unsigned char data_0209f5c4[12];
unsigned char data_0209f5dc[12];
}
DSSTATE_END

/* ---- rung C1d: _ZTV7Clipper and one more destruct-node cell ---------------
 *
 * The header carries the derivation. The trap is defined before the table
 * because the table takes its address. */

namespace {
void ctor_trap_clipper_d0(void)
{
    std::fprintf(stderr, "  UNHOSTED: _ZTV8dClipper slot 1, _ZN8dClipperD0Ev at "
                         "0x020156fc -- src/_ZN7ClipperD0Ev.cpp calls "
                         "base_dtor_Clipper, an invented name for 0x0203cbcc "
                         "that nothing in this tree defines. The only Clipper "
                         "in this image is the static data_0209f43c and "
                         "nothing deletes it, so this slot is unreachable.\n");
    std::fflush(stderr);
}
}  /* anonymous namespace */

DSSTATE_BEGIN
extern "C" {
void *_ZTV8dClipper[2] = {
    (void *)&_ZN8dClipperD1Ev,
    (void *)&ctor_trap_clipper_d0,
};
unsigned char data_0209f388[12];
}
DSSTATE_END

/* ---- rung C1e: the 38 camera-state pairs ---------------------------------
 *
 * arm9 .rodata 0x02086bc8..0x02086e48, eight bytes each, sorted by address.
 * The comment on each line names the State object and the half it fills. The
 * derivation, the equality proof against hal/camera_states.cpp and the reason
 * these words stay DS addresses are all in the header.
 *
 * THE BYTES COME FROM THE PLAYER'S ROM. Each pair is the ROM's own word (a
 * relocated code address, which is the same DS address in the image because
 * arm9 loads at its link base) and a zero this-delta. port/tools/romdata.py
 * emits all 38 as NAMED rows from extracted/arm9_dec.bin, inside .dsstate, and
 * a ROM-clean build fills them from romdata.bin inside main(), in
 * port_romdata_load(), before port_rom_entry_ctors runs __sinit_d_camera.cpp
 * (whose body is below). This file only declares them; until lane ROMDATA it
 * carried the 38 words as literals. */
DSSTATE_BEGIN
extern "C" {
extern unsigned int data_02086bc8[2];  /* data_0209b0b8 main */
extern unsigned int data_02086bd0[2];  /* data_0209b0d8 onEnter */
extern unsigned int data_02086be0[2];  /* data_0209b098 main */
extern unsigned int data_02086be8[2];  /* data_0209b098 onEnter */
extern unsigned int data_02086bf0[2];  /* data_0209b068 main */
extern unsigned int data_02086bf8[2];  /* data_0209b048 main */
extern unsigned int data_02086c00[2];  /* data_0209b0a8 main */
extern unsigned int data_02086c08[2];  /* data_0209b0b8 onEnter */
extern unsigned int data_02086c10[2];  /* data_0209b088 onEnter */
extern unsigned int data_02086c18[2];  /* data_0209b0c8 onEnter */
extern unsigned int data_02086c28[2];  /* data_0209b008 main */
extern unsigned int data_02086c30[2];  /* data_0209b018 main */
extern unsigned int data_02086c40[2];  /* data_0209b028 main */
extern unsigned int data_02086c48[2];  /* data_0209b028 onEnter */
extern unsigned int data_02086c58[2];  /* data_0209b038 main */
extern unsigned int data_02086c68[2];  /* data_0209b048 onEnter */
extern unsigned int data_02086c70[2];  /* data_0209b068 onEnter */
extern unsigned int data_02086c98[2];  /* data_0209b038 onEnter */
extern unsigned int data_02086ce0[2];  /* data_0209b0e8 onEnter */
extern unsigned int data_02086d10[2];  /* data_0209b0a8 onEnter */
extern unsigned int data_02086d28[2];  /* data_0209b008 onEnter */
extern unsigned int data_02086d30[2];  /* data_0209b078 main */
extern unsigned int data_02086d48[2];  /* data_0209b058 main */
extern unsigned int data_02086d58[2];  /* data_0209b058 onEnter */
extern unsigned int data_02086d90[2];  /* data_0209b088 main */
extern unsigned int data_02086da8[2];  /* data_0209b018 onEnter */
extern unsigned int data_02086db0[2];  /* data_0209b0c8 main */
extern unsigned int data_02086db8[2];  /* data_0209b078 onEnter */
extern unsigned int data_02086dc8[2];  /* data_0209b128 main */
extern unsigned int data_02086dd0[2];  /* data_0209b128 onEnter */
extern unsigned int data_02086de0[2];  /* data_0209b118 main */
extern unsigned int data_02086de8[2];  /* data_0209b118 onEnter */
extern unsigned int data_02086df8[2];  /* data_0209b108 main */
extern unsigned int data_02086e00[2];  /* data_0209b108 onEnter */
extern unsigned int data_02086e10[2];  /* data_0209b0f8 main */
extern unsigned int data_02086e18[2];  /* data_0209b0f8 onEnter */
extern unsigned int data_02086e28[2];  /* data_0209b0e8 main */
extern unsigned int data_02086e40[2];  /* data_0209b0d8 main */
}
DSSTATE_END

/* THE ROM INITIALISER'S BODY. __sinit_d_camera.cpp makes 19 copies: each State
   is {onEnter pair, main pair}, in src/engine/view/d_camera.cpp's order. That
   TU's own dynamic initialisers make the same copies from .CRT$XCU before
   main, which is too early for a ROM-clean build: there the 38 pairs are zero
   until port_romdata_load() fills them from romdata.bin inside main. The ROM
   walk runs after that fill, so redoing the copies here is what seats the
   States in a kit build. In a build with baked tables it rewrites the bytes
   already there. The table below is d_camera.cpp's 19 initialisers, row for
   row. */
extern "C" void __sinit_02073a24(void)
{
    struct Row { unsigned int *st; const unsigned int *on, *mn; };
    static const Row k[19] = {
        { data_0209b008, data_02086d28, data_02086c28 }, { data_0209b018, data_02086da8, data_02086c30 },
        { data_0209b028, data_02086c48, data_02086c40 }, { data_0209b038, data_02086c98, data_02086c58 },
        { data_0209b048, data_02086c68, data_02086bf8 }, { data_0209b058, data_02086d58, data_02086d48 },
        { data_0209b068, data_02086c70, data_02086bf0 }, { data_0209b078, data_02086db8, data_02086d30 },
        { data_0209b088, data_02086c10, data_02086d90 }, { data_0209b098, data_02086be8, data_02086be0 },
        { data_0209b0a8, data_02086d10, data_02086c00 }, { data_0209b0b8, data_02086c08, data_02086bc8 },
        { data_0209b0c8, data_02086c18, data_02086db0 }, { data_0209b0d8, data_02086bd0, data_02086e40 },
        { data_0209b0e8, data_02086ce0, data_02086e28 }, { data_0209b0f8, data_02086e18, data_02086e10 },
        { data_0209b108, data_02086e00, data_02086df8 }, { data_0209b118, data_02086de8, data_02086de0 },
        { data_0209b128, data_02086dd0, data_02086dc8 },
    };
    for (int i = 0; i < 19; ++i) {
        std::memcpy(k[i].st, k[i].on, 8);
        std::memcpy(k[i].st + 2, k[i].mn, 8);
    }
}

/* The five MSVC spellings src/__sinit_02074e84.cpp emits, transcribed from
   dumpbin /symbols on this tree's own compile of it. */
#pragma comment(linker, "/alternatename:?data_0208ee14@@3DA=_data_0208ee14")
#pragma comment(linker, "/alternatename:?_ZTVN10dScStage_c15graphCallback_cE@@3DA=__ZTVN10dScStage_c15graphCallback_cE")
#pragma comment(linker, "/alternatename:?data_0209f388@@3DA=_data_0209f388")
#pragma comment(linker, "/alternatename:?sStageGraphCallback@@3RAXA=_sStageGraphCallback")
#pragma comment(linker, "/alternatename:?data_0209f43c@@3DA=_data_0209f43c")

namespace {

/* ---- the counters -------------------------------------------------------- */

int g_hits_02073e6c;
int g_hits_02074f80;
int g_faces_reported;
int g_rom_bodies_run;

/* ---- the two counting shims ---------------------------------------------- */

void ctor_02073e6c(void)
{
    ++g_hits_02073e6c;
    __sinit_02073e6c();
}

void ctor_02074f80(void)
{
    ++g_hits_02074f80;
    __sinit_02074f80();
}

/* ---- the thirteen faces --------------------------------------------------
 *
 * One function per initialiser so the name in the message is the name of the
 * word that reached it, not a shared string with an argument. Each reports
 * once. They RETURN: see the header for why an abort at word 0 would be a
 * boot that never starts rather than a fact anybody could act on. */

void ctor_face(const char *name, const char *why)
{
    ++g_faces_reported;
    std::fprintf(stderr, "  [ctor] NOT LINKED: %s -- %s\n", name, why);
}

#define CTOR_FACE(sym, why)                                                    \
    void ctor_face_##sym(void)                                                 \
    {                                                                          \
        static int said;                                                       \
        if (said) { ++g_faces_reported; return; }                              \
        said = 1;                                                              \
        ctor_face("__sinit_" #sym, why);                                       \
    }

CTOR_FACE(02074e0c, "func_0201aa18 -> func_0201aad4 -> func_0201aac8 is an argument-dropping tail-call veneer chain into OS_CreateAlarm; a PORT_HOST_ABI question, not a linkage one")
/* __sinit_fListNode.cpp IS TAKEN (run linkfull, lane K1LOOP). Its face read
   "data_02099f48..data_02099f70 are hosted nowhere -- five mwcc
   pointer-to-member pairs -- and hal/actor_registry.cpp already seats the same
   five list callbacks with host wrappers, LATER in the boot". Both halves are
   gone: unmatched/func_02043fdc_hostcopy.cpp hosts the five records (each
   {the list's __fastcall face, 0}, the pair PMF3 proved), and
   port_actor_lists_seat only checks the heads now. The ROM's body is the last
   word of the table and fills the five list heads _Z13func_02044120v walks. */

#undef CTOR_FACE

/* ---- the table -----------------------------------------------------------
 *
 * The ROM's 23 words in the ROM's order, plus the ROM's NULL terminator. Word
 * N here IS the word at 0x02086b60 + 4*N, and the comment on each line is the
 * DS address that word holds in the cartridge. */

typedef void (*ctor_fn)(void);

struct CtorWord {
    unsigned    ds;       /* what the ROM's word holds */
    ctor_fn     host;     /* what this build binds it to */
    const char *name;     /* the initialiser's name, for the log */
    int         rom;      /* 1 = the ROM's own body runs, 0 = a face */
};

const CtorWord kCtorTable[] = {
    { 0x02073a24, __sinit_02073a24, "__sinit_d_camera.cpp", 1 },
    { 0x02073e6c, ctor_02073e6c,      "__sinit_KuppaScript.cpp", 1 },
    { 0x02074d90, __sinit_02074d90,   "__sinit_d_actor.cpp", 1 },
    { 0x02074da8, __sinit_02074da8,   "__sinit_Sound.cpp", 1 },
    { 0x02074dbc, __sinit_02074dbc,   "__sinit_GetLevelPart.cpp", 1 },
    { 0x02074dc0, __sinit_02074dc0,   "__sinit_SublevelToLevel.cpp", 1 },
    { 0x02074dc4, __sinit_02074dc4, "__sinit_Timer.cpp", 1 },
    { 0x02074e0c, ctor_face_02074e0c, "__sinit_func_0201a96c.cpp", 0 },
    { 0x02074e44, __sinit_02074e44, "__sinit_func_0201fec8.cpp", 1 },
    { 0x02074e80, __sinit_02074e80,   "__sinit_func_0202043c.cpp", 1 },
    { 0x02074e84, __sinit_02074e84, "__sinit_d_s_stage_methods.cpp", 1 },
    { 0x02074edc, __sinit_02074edc, "__sinit_d_scene.cpp", 1 },
    { 0x02074f80, ctor_02074f80,      "__sinit_d_wipe.cpp", 1 },
    { 0x02074fb8, __sinit_02074fb8,   "__sinit_d_s_mb.cpp", 1 },
    { 0x02074fe4, __sinit_02074fe4, "__sinit_d_bg_w_kc.cpp", 1 },
    { 0x0207501c, __sinit_0207501c, "__sinit_d_bg_w_kc_mbg.cpp", 1 },
    { 0x02075054, __sinit_02075054, "__sinit_Vec3_ApproachHorz.cpp", 1 },
    { 0x020750b4, __sinit_020750b4,   "__sinit_func_0203bb5c.cpp", 1 },
    { 0x020750b8, __sinit_020750b8, "__sinit_Heap.cpp", 1 },
    { 0x020750ec, __sinit_020750ec, "__sinit_Vec2_Len.cpp", 1 },
    { 0x0207511c, __sinit_0207511c, "__sinit_cstd_sqrt.cpp", 1 },
    { 0x02075150, __sinit_02075150,   "__sinit_unit0203d7b8.cpp", 1 },
    { 0x02075154, __sinit_02075154,   "__sinit_fListNode.cpp", 1 },
};
const unsigned kCtorWords = sizeof kCtorTable / sizeof kCtorTable[0];

/* The ROM's own address of the table and of its terminator word. */
const unsigned kCtorBase = 0x02086b60u;

/* How long the MSL global-destructor chain is. Nodes are three words and
   node[0] is the previous head (src/func_020731dc.c), so this is a walk, not a
   counter somebody has to keep in step. The cap is a cycle guard: a corrupt
   chain must not hang the boot. */
int dtor_chain_len(void)
{
    int n = 0;
    void **p = (void **)__global_destructor_chain;
    while (p && n < 4096) { p = (void **)p[0]; ++n; }
    return n;
}

/* The nineteen camera State objects, in ROM order, for the C1e redundancy
   check. Nineteen named symbols rather than one array walk: those are nineteen
   separate definitions in hal/camera_states.cpp and only the linker decides
   they are adjacent. */
unsigned int *const kCamStates[19] = {
    data_0209b008, data_0209b018, data_0209b028, data_0209b038, data_0209b048,
    data_0209b058, data_0209b068, data_0209b078, data_0209b088, data_0209b098,
    data_0209b0a8, data_0209b0b8, data_0209b0c8, data_0209b0d8, data_0209b0e8,
    data_0209b0f8, data_0209b108, data_0209b118, data_0209b128,
};

void cam_states_snapshot(unsigned int *out)
{
    for (int i = 0; i < 19; ++i)
        for (int j = 0; j < 4; ++j)
            out[i * 4 + j] = kCamStates[i][j];
}

int cam_states_changed(const unsigned int *before)
{
    int n = 0;
    for (int i = 0; i < 19; ++i)
        for (int j = 0; j < 4; ++j)
            if (kCamStates[i][j] != before[i * 4 + j]) ++n;
    return n;
}

/* Is the ROM's table address a committed, writable page in this process? */
bool ctor_page_ok(void)
{
#if defined(_WIN32)
    MEMORY_BASIC_INFORMATION mbi;
    std::memset(&mbi, 0, sizeof mbi);
    if (!VirtualQuery((LPCVOID)(size_t)kCtorBase, &mbi, sizeof mbi))
        return false;
    if (mbi.State != MEM_COMMIT)
        return false;
    const DWORD w = PAGE_READWRITE | PAGE_WRITECOPY |
                    PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    if ((mbi.Protect & w) == 0)
        return false;
    /* The whole table plus the terminator has to be inside the same region. */
    const size_t need = (size_t)(kCtorWords + 1) * 4u;
    const size_t have = (size_t)mbi.RegionSize -
                        ((size_t)kCtorBase - (size_t)mbi.BaseAddress);
    return have >= need;
#else
    return false;
#endif
}

}  /* anonymous namespace */

extern "C" void port_rom_entry_ctors(void)
{
    static int done;
    if (done) {
        std::fprintf(stderr, "  [ctor] port_rom_entry_ctors called twice; the "
                             "second call did nothing\n");
        return;
    }
    done = 1;

    const int dtors_before = dtor_chain_len();
    unsigned int cam_before[19 * 4];
    cam_states_snapshot(cam_before);
    const bool page = ctor_page_ok();

    if (page) {
        unsigned *w = (unsigned *)(size_t)kCtorBase;
        for (unsigned i = 0; i < kCtorWords; ++i)
            w[i] = (unsigned)(size_t)kCtorTable[i].host;
        w[kCtorWords] = 0;                 /* the ROM's own terminator */
        __call_static_initializers();                   /* Entry's own call, 0x020048a8 */
    } else {
        std::fprintf(stderr, "  [ctor] DS main RAM at %08x is not committed "
                             "and writable, so the ROM's runner cannot walk "
                             "its own table; running the 23 words directly. "
                             "See ntr/io.cpp: main RAM is the one reserved "
                             "range a process may lose.\n", kCtorBase);
        for (unsigned i = 0; i < kCtorWords; ++i)
            kCtorTable[i].host();
    }

    for (unsigned i = 0; i < kCtorWords; ++i)
        if (kCtorTable[i].rom) ++g_rom_bodies_run;

    const int dtors_after = dtor_chain_len();

    std::fprintf(stderr,
                 "[ctor] __call_static_initializers walked %u words at %08x (%s): %d ran as "
                 "ROM bodies, %d as faces; __sinit_KuppaScript.cpp hits %d, "
                 "__sinit_d_wipe.cpp hits %d; dtor chain on __global_destructor_chain "
                 "%d -> %d\n",
                 kCtorWords, kCtorBase,
                 page ? "the ROM's own runner" : "host fallback walk",
                 g_rom_bodies_run, (int)kCtorWords - g_rom_bodies_run,
                 g_hits_02073e6c, g_hits_02074f80,
                 dtors_before, dtors_after);

    /* RUNG C1e, CHECKED RATHER THAN CLAIMED. __sinit_d_camera.cpp copies the
       38 ROM pairs into the nineteen State objects. Before the walk those
       objects hold what d_camera.cpp's own static initialisers copied before
       main: the same words in a build with baked tables (changed 0 is the
       pass there), zeros in a ROM-clean build, where the pairs are filled
       from romdata.bin only inside main (changed 38, every non-zero word,
       is the pass there). Either way the States must end with every main
       word set, or the camera state machine never runs; that is checked
       loudly on its own. */
    {
        const int changed = cam_states_changed(cam_before);
        int nonzero = 0, main_set = 0;
        for (int i = 0; i < 19; ++i) {
            for (int j = 0; j < 4; ++j)
                if (kCamStates[i][j]) ++nonzero;
            if (kCamStates[i][2]) ++main_set;
        }
        std::fprintf(stderr, "  [ctor] camera-state fill: __sinit_d_camera.cpp "
                             "wrote 76 words and changed %d of them (0 expected "
                             "with baked tables, 38 in a ROM-clean build); "
                             "%d of 76 now non-zero\n", changed, nonzero);
        if (main_set != 19 || (changed != 0 && changed != 38))
            std::fprintf(stderr, "  [ctor] CAMERA-STATE FILL WRONG: %d of 19 "
                                 "States have a main word, %d of 76 words "
                                 "changed; the camera will not run\n",
                         main_set, changed);
    }

    /* THE ONE THING A BYTE GATE CANNOT SEE, kept after rung C1c came back out.
       When __sinit_d_scene.cpp is bound to the ROM's body it is the last writer
       of both fader vptrs, after hal/fader_wipes.cpp's gate-31 placement-new
       has already run; when it is faced, as it is now, the colour fader keeps
       the host HalFaderWipe's table and gSoftResetBrightnessFader keeps a null vptr.
       Naming the table each one landed on is what makes either state visible
       in every captured run instead of only in this file's prose, and it is
       how the regression above was read. */
    /* SYNC093: main's dWipe_c is a real class, so the ROM walk's constructor (__sinit_d_wipe.cpp -> dWipe_c::dWipe_c) stores the
       COMPILED vftable. The scene code reaches this object through the ROM-numbered, flat-faced view (hal/fdr_arm9_fader_seat.cpp)
       and would call the member functions with the wrong convention (this in ECX, nothing on the stack). Name the seated table. */
    *(void **)(void *)&data_0209f61c = (void *)_ZTV7dWipe_c;
    {
        const void *cv = *(void *const *)(const void *)gSceneColorFader;
        const void *bv = *(void *const *)(const void *)gSoftResetBrightnessFader;
        std::fprintf(stderr,
                     "  [ctor] fader vptrs after the walk: gSceneColorFader -> %s, "
                     "gSoftResetBrightnessFader -> %s\n",
                     cv == (const void *)_ZTV10dFdColor_c ? "_ZTV10dFdColor_c "
                         "(_ZTV10dFdColor_c, ten slots seated in "
                         "hal/scene_boot.cpp)"
                     : cv == (const void *)_ZTV15dFdBrightness_c ? "_ZTV15dFdBrightness_c"
                     : cv == (const void *)_ZTV8dFader_c ? "_ZTV8dFader_c"
                     : cv == 0 ? "NULL"
                     : "a table this file does not name (the host "
                       "HalFaderWipe's, or something else)",
                     bv == (const void *)_ZTV15dFdBrightness_c ? "_ZTV15dFdBrightness_c "
                         "(_ZTV15dFdBrightness_c, dtor pair seated and every "
                         "slot from +0x08 up a named trap)"
                     : bv == (const void *)_ZTV8dFader_c ? "_ZTV8dFader_c"
                     : bv == 0 ? "NULL"
                     : "a table this file does not name");

        /* AND WHETHER THE TABLE IT LANDED ON WAS LIVE WHEN IT LANDED, which is
           the whole of what the first landing of rung C1c got wrong. Ten live
           words and zero dead ones is the pass. */
        const int seated = port_fader_vtable_seated();
        int live = 0;
        for (int i = 0; i < 10; ++i) if (_ZTV10dFdColor_c[i]) ++live;
        if (seated && live == 10 && port_fader_vtable_zero_words() == 0)
            std::fprintf(stderr, "  [ctor] fader vtable seat check PASS: "
                         "hal/scene_boot.cpp seated its four fader tables "
                         "before main, all ten words of _ZTV10dFdColor_c are "
                         "live at the walk, 0 zero words across the 34 it "
                         "owns\n");
        else
            std::fprintf(stderr, "  [ctor] FADER VTABLE SEAT NOT LIVE AT THE "
                         "WALK: seated %d, %d of _ZTV10dFdColor_c's ten words "
                         "live, %d zero words across the 34. An installed "
                         "fader dispatching this table would call address 0, "
                         "which is the fault run link100 lane WM9 caught\n",
                         seated, live, port_fader_vtable_zero_words());
    }

    if (g_hits_02073e6c != 1 || g_hits_02074f80 != 1)
        std::fprintf(stderr, "  [ctor] ONCE-ONLY CHECK FAILED: the two "
                             "initialisers that used to be hand-called ran "
                             "%d and %d times, not 1 and 1\n",
                     g_hits_02073e6c, g_hits_02074f80);
    else
        std::fprintf(stderr, "  [ctor] once-only check PASS: __sinit_KuppaScript.cpp "
                             "1, __sinit_d_wipe.cpp 1\n");
}
