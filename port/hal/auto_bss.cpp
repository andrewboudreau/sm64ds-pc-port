// GENERATED-ACCUMULATED zero storage for gate-10/11 BSS ring symbols.
// Grown by the link-sweep loop; sizes are generous defaults.
//
// These are hosted DS BSS globals -- mutable game state (message and cutscene
// mode flags, the Process cleanup list, the particle cursors, and much more) --
// so they are routed into the .dsstate section the save state captures. The
// DSSTATE_BEGIN/END markers set the default bss/data segment for every global
// between them; the link-sweep loop that grows this file emits new symbols
// inside the bracket, so anything it adds is captured with no further work. See
// hal/dsstate_seg.h.
#include <cstdio>
#include "dsstate_seg.h"
// run vs16: the one place the port's player width is written down.
DSSTATE_BEGIN
extern "C" {
/* MSG_GEN_TEXT_FUNCS is NOT zeroed storage: it is a 3-entry function-pointer
   table _ZN7Message10LayoutLineEv calls through on a 0xfe message control byte. Seated with
   real host addresses in hal/message_gen_text.cpp -- a zeroed array here was a
   null call (eip=0) on every multi-page line carrying an embedded-text escape. */
int VT1[8];
/* VS wiring lane (ov075): the VS menu's selection block (map pick at b2e4, an
   allocation pointer and two neighbours) and two wireless-status bytes.
   Generous [8] is the file's own convention; the ROM spans are 4 each. */
int data_0209b2e4[8];
/* PORTSYNC3: GX.cpp texture / palette bank selectors (BeginLoadTex / BeginLoadTexPltt). */
int data_020a60b4[8];
int data_020a60b8[8];
int data_0209b2e8[8];
/* data_0209b2ec MOVED to hal/scene_vs_menu.cpp (run rel0215 lane prop15), for
   the reason gNumStageStars moved out of hal/actor_vtables.cpp: dsd splits the
   four-byte run at 0x0209b2ec into data_0209b2ec (1 byte) and data_0209b2ed
   (3), src/func_ov075_021165b0.c walks BOTH names over those same four bytes,
   and a split run cannot be hosted as two disjoint objects. It is a
   grouped-section pair there, gated by gxband_guard's 'vsrank' band. */
int data_0209b2f0[8];
int data_0209fc54[8];
int data_0209fc58[8];
int data_0209fcfc[8];
int data_0209fd04[8];
int data_0209fc80[8];
int data_0209fcf4[8];
int data_0209fd18[8];
int data_0209fc70[8];
int data_0209fc90[8];
int data_0209fc98[8];
int data_0209fca4[8];
int data_0209fca8[8];
int data_0209fcbc[8];
int data_0209fcc4[8];
int data_0209fcd0[8];
int data_0209fcd4[8];
int data_0209fcf8[8];
int data_0209fd00[8];
int data_0209fd08[8];
int data_0209fd0c[8];
int data_0209fd10[8];
int data_0209fd14[8];
/* SIZED BY ROM SPAN, run link100 lane BSSFIX. The width column of the same
   60-character text buffer as data_0209fd5c below: _ZN7Message9AddCharVSEh writes one
   byte per character at data_0209fd20[data_0209fcd8] and advances that counter
   with NO bound, and _ZN7Message12LayoutLineVSEv resets the counter beside data_0209fd1c.
   config/arm9/symbols.txt puts the next symbol (data_0209fd5c) 0x3c on, so the
   ROM object is 60 bytes, one width per character -- and the generous int[8]
   default held 32 of the 60, so character 33 onward wrote past it. This is the
   same pair as data_0209d710 / data_0209d74c seventy lines below, which was
   fixed in 8f09d891e; this copy of the buffer was missed. */
int data_0209fd20[0x3c / 4];    /* ROM span 0x3c -- one width byte per character */
int data_020a0f40[8];
int data_020a0f70[8];
/* data_020a6148 MOVED to the link100 GLOBALS block at the foot of
   hal/cxx_aliases.cpp. It is NOT storage of its own: 0x020a6148 is
   OSi_ThreadInfo + 0x14, the sixteen-slot thread table inside the ROM's one
   0x54-byte OSThreadInfo record, and src/func_02058538.c reaches those same
   slots through the OTHER name. Two host objects meant the allocator wrote one
   array while the scanner read the other -- silently, with nothing to fault.
   The three names are a grouped run there; the measurement is in that block. */
/* THE OVERLAY-RESIDENT TABLE, and the generous-default convention was 112
   bytes short of it. ROM span from config/arm9/symbols.txt is
   0x0209d454 - 0x0209d3c4 = 0x90 = 144 bytes, which is exactly the twelve
   12-byte records src/func_02017e60.c writes:

       for (i = 0; i < 12; i++, p++) { p->a = -1; p->b = 0; p->c = 0; }

   src/LoadOverlay.c and src/func_02017e94.c walk the same twelve. At int[8]
   that loop ran 112 bytes past the end of the object and over whatever this
   file's link-sweep loop had put next to it. func_02018aa4, the ROM's own
   file-system bring-up in src/func_0201a054.c, is the call that reaches it. */
int data_0209d3c4[144 / 4];
int data_0209d4f8[8];
int data_0209d518[8];
/* THE CAROUSEL WORKER'S THREAD OBJECT, and the generous default was 116 bytes
   short of it -- the data_0209d3c4 shape above, one more time. (run link100,
   lane DF40.)

   ROM span from config/arm9/symbols.txt is 0x0209d64c - 0x0209d5b8 = 0x94 =
   148 bytes, and that is the DS thread record src/func_02058200.c builds:
   src/func_02058158.c reads p[25] (offset 0x64, the state word),
   src/func_02057f38.c writes offset 0x84/0x8c, src/func_020581a8.cpp reaches
   the wakeup queue at 0x90. At int[8] every one of those was 0x44 or more
   bytes past the end of the object and over whatever this file's link-sweep
   loop had put next to it.

   IT HAS NOT FAULTED YET only because all four reachers are faced idle in
   hal/scene_vs_menu.cpp's refused-subtree block, so nothing has read or
   written past offset 0. This lane seats the first of them -- the poll,
   src/func_02058158.c, whose only linked caller src/func_0201a1bc.c passes
   exactly this object -- and that read at 0x64 becomes real, which is what
   turns the size from a tidy-up into a correctness question. The other three
   stay faced (they need the ROM scheduler, which needs the ARM context
   switch). Sized by the whole ROM span rather than by 0x68 on purpose, so the
   day one of those three lands this does not have to be found again. */
int data_0209d5b8[148 / 4];
int data_0209f278[8];
int data_0209fc60[8];
int data_0209fc78[8];
int data_0209fc7c[8];
int data_0209fc84[8];
int data_0209fc88[8];
int data_0209fc94[8];
int data_0209fca0[8];
int data_0209fcb0[8];
int data_0209fccc[8];
int data_0209fcd8[8];
int data_0209fcdc[8];
int data_0209fce8[8];
int data_0209fd1c[8];
/* SIZED BY ROM SPAN, run link100 lane BSSFIX. The VS / wireless text buffer,
   and byte-for-byte the data_0209d74c fault below repeating on its sibling:
   src/func_02031e00.c:37 and src/func_02033464.c:51 both open it with
   MIi_CpuClear32(<val>, &data_0209fd5c, 0xf00), and MIi_CpuClear32's len is a
   BYTE count (hal/heap_globals.cpp), so both clear 3840 bytes. The int[8]
   default gave them 32, so every call wiped 3808 bytes of whatever the linker
   put next -- the same shape that took out hal_wipes[0]'s vptr and faulted
   HUD::Behavior the first time Mario talked.
   THE SIZE IS 0xf00, NOT the 0x4e8 delta to the next symbol. config/arm9 splits
   this one ROM object in two: data_0209fd5c, then data_020a0244 at +0x4e8, then
   sPrevHeap$390 -- and 0x0209fd5c + 0xf00 IS 0x020a0c5c exactly, so the fill
   lands on a symbol boundary and 0xf00 is the measured object, not a guess. It
   stays inside .bss (0x0209b000..0x020aa420). data_020a0244 is hosted nowhere
   and referenced nowhere, so this double-hosts nothing; checked, because that is
   the trap next door.
   tools/arm9_globals_guard.py now fails the build on this class instead of
   asking a reader to remember to re-audit. */
int data_0209fd5c[0xf00 / 4];   /* ROM span 0xf00 -- the VS text buffer */
int gNextActorUpdateMask[8];
int sVolumeRampStep[8];
int data_0209cee8[8];
/* catchup1 (SYM4, needs row from CUST4): decomp main names this .bss word
   dCc_c::listHead (config/arm9 _ZN5dCc_c8listHeadE, 0x0209cee8) and no TU
   defines it. The static member resolves onto this one storage, so the
   readers still spelling data_0209cee8 and the ones on dCc_c::listHead walk
   the same list. */
#pragma comment(linker, "/alternatename:?listHead@dCc_c@@2PAU1@A=_data_0209cee8")
int gSubScreenLayers[8];
int gMainScreenLayers[8];
int gSubBg1VOffset[8];
int gSubBg1HOffset[8];
int gBg3HOffset[8];
int gBg3VOffset[8];
int gBg1VOffset[8];
int gBg1HOffset[8];
/* SIZED BY ROM SPAN, not by the generous default. The eight symbols marked
   this way are the ones whose delta to the next ROM symbol is bigger than the
   32-byte default -- they are buffers, and a ROM routine that writes the WHOLE
   object runs off the end of the host copy and into whatever the linker put
   next. _ZN7Message10LayoutLineEv opens the message box with
   MIi_CpuClear32(0, &data_0209d74c, 0xf00) and cleared 3808 bytes past the
   default straight through hal_wipes[0]'s vtable pointer, which HUD::Behavior
   then dispatched IsAtStart on. Same class as the undersized data_0209f4ae
   that strayed into MovingCylinderClsn's vtable (hal/actor_vtables.cpp).
   Re-audit with the delta-to-next-symbol rule after adding any symbol here. */
int data_0209f27c[8];
unsigned char data_0209f4a2[0x18 * 4];
unsigned char data_0209f4a4[0x18 * 4];
unsigned char data_0209f4a6[0x18 * 4];
int data_0209f498[(0x18 * 4) / 4];
int data_0209f350[4];
int data_0209f33c[4];
/* data_0209fc5c MOVED to hal/scene_vs_menu.cpp (fc5c width reconciliation),
   for the reason data_0209b2ec moved: dsd splits the four-byte run at
   0x0209fc5c into data_0209fc5c (1 byte) and data_0209fc5d (3), the ROM walks
   both names over those same four BYTES, and the generic int[8] here let
   hal/level_boot.cpp seat it at int stride -- which read as byte
   fc5c[1] == 0 and froze player 2 on every VS map (daPly_c::Behavior's VS
   gate). Grouped-section pair there, gated by gxband_guard's 'ready' band. */
int data_0209fc68[8];
int data_020a0d84[8];
int data_020a0d88[8];
int data_020a0db0[8];
/* run mg11 lane TTE: the two shorts dScMgTrampoline2_c's and
   dScMgTrampoline_c's slot 18 seed their stroke endpoints from
   (src/minigames/d_s_mg_trampoline2.cpp and src/minigames/d_s_mg_trampoline.cpp both read
   data_020a0dbc[0] and [1] with ldrsh, and src/__sinit_02075054.c is what
   writes them). SIZED BY ROM SPAN rather than by the generous default above:
   config/arm9/symbols.txt puts the next symbol at 0x020a0dc0, four bytes on,
   which is exactly the two halfwords the three TUs read. */
short data_020a0dbc[2];
/* ---- THE STYLUS RECORD, 0x020a0de8 .. 0x020a0df7 -------------------------
   ONE 16-byte block, not three independent arrays.

   On hardware this is TouchInfo[4] -- one 4-byte record per controller,
   {u8 touched, u8 edge, u8 x, u8 y} -- and dsd named a symbol at each of the
   FIRST record's four bytes, because code reached the fields through separate
   addresses. config/arm9/symbols.txt has the four one byte apart

       data_020a0de8 0x020a0de8   (+0 touched)
       data_020a0de9 0x020a0de9   (+1 edge, touch ^ prev)
       data_020a0dea 0x020a0dea   (+2 x)
       data_020a0deb 0x020a0deb   (+3 y)

   the next named symbol is data_020a0df8, and delinks.txt puts all of it in
   .bss (0x0209b000..0x020aa420), so the block is exactly 16 bytes = 4
   records and there are no image bytes to read: the LAYOUT evidence is the
   readers. Every one of them indexes ITS OWN name by slot*4 --
   Message::Update's data_020a0de9[idx * 4], Stage::CheckCameraInput's
   data_020a0de8[i].held, _Z16TouchArea_UpdatePvi's data_020a0dea[i4] -- so the four
   names have to BE the same storage, one byte apart.

   WHAT WAS HERE BEFORE. The link sweep grew three separate `int [8]` arrays
   (de8, de9, deb) and nothing at all for dea. hal/sub_screen.cpp writes the
   packed record through data_020a0de8, so Message::Update's
   data_020a0de9[idx*4] and data_020a0deb[idx*4] read two other arrays that
   nothing ever writes: measured with SM64DS_TOUCH_PROBE before this change,
   the block held 01 01 78 96 while de9 and deb read 0, and the two names sat
   +32 and +64 bytes from de8 instead of +1 and +3. `held` and `y` were zero
   on every frame of every run. Same class as the pad pair data_020a0e58 /
   data_020a0e5a that hal/input_probe.cpp works around by writing both; this
   fixes the storage instead.

   HOW THE LAYOUT IS RESTORED. The mechanism tools/ovdata.py uses to
   reproduce ROM spacing and dsstate_guard names in its own fix-it text: one
   ordered .dsstate$<family><NNNN> slot per symbol, each with its ROM
   alignment, so the linker concatenates the four into one contiguous run.
   $touch0000..0003 sorts after the $aaa sentinel and before $zzz, so the run
   stays inside the captured span -- the stylus record is DS state a save
   state has to roll back. Sizes are the DS symbol spacing (next symbol minus
   this one) and the LAST name owns the rest of the block, which is ovdata's
   rule too.

   DO NOT give these "sizes that look right" (de8[16], de9[16], ...). They
   OVERLAP by construction, and MSVC treats two named globals as disjoint
   objects: a TU that stores through one and loads through another IN THE
   SAME FUNCTION can have the load folded away. Nothing in the game path can
   hit it -- the one writer only stores, every reader only loads.

   VOLATILE IS NOT THE GUARD, and an earlier revision of this comment said it
   was. A review disassembled a probe built on that premise and found MSVC
   hoisting all three volatile sibling loads ABOVE the store, because volatile
   orders accesses to the SAME object and these names are disjoint objects to
   the compiler. sub_screen's probe happens to emit its pre-reads, then the
   stores, then its post-reads, so it reports honestly today -- but anything
   new that needs a store on one name to be seen through another must use
   _ReadWriteBarrier() (or read through the same name it wrote), not volatile. */
__pragma(section(".dsstate$touch0000", read, write)) __declspec(allocate(".dsstate$touch0000")) __declspec(align(1)) unsigned char data_020a0de8[1] = { 0 };
__pragma(section(".dsstate$touch0001", read, write)) __declspec(allocate(".dsstate$touch0001")) __declspec(align(1)) unsigned char data_020a0de9[1] = { 0 };
__pragma(section(".dsstate$touch0002", read, write)) __declspec(allocate(".dsstate$touch0002")) __declspec(align(1)) unsigned char data_020a0dea[1] = { 0 };
__pragma(section(".dsstate$touch0003", read, write)) __declspec(allocate(".dsstate$touch0003")) __declspec(align(1)) unsigned char data_020a0deb[4 * 4 - 3] = { 0 };
int data_020a0e5a[4 * 2];
/* data_020a1052 moved to hal/camera_bridges.cpp: it is a field INSIDE the
   local comms record at data_020a1040, not storage of its own */
/* data_0209cab4 moved to hal/level_boot.cpp: it is the second symbol of
   the save block, which the entrance loader reads across */
/* data_0209e650 (the LevelRng seed) is no longer hosted here: main's
   src/unnamed/arm9/0201/func_0201fec8.cpp defines the 4-byte LevelRng object and
   seeds it in its own initialiser on the host too (catchup1 Stage D, with ENG4's
   patch dropping the PC guard); the C and int spellings alias onto it from
   hal/cxx_aliases.cpp. */
/* 0x0209f40c is STAR_MARKERS, which src/game/stages/d_s_stage_methods.cpp
   defines; the minimap host copy, level_boot and a host-side caller name it directly.
   The old address name data_0209f40c is gone from config/ and from the port. */
/* gSceneFader (the installed fader) moved to hal/fader_wipes.cpp: the
   two FUN_0202xxxx wipe helpers deref it with no null check. */
int gSavedKuppaScript[8];
int data_020a0e58[4 * 2];
int data_0209b004[8];
int data_0209b138[0x138 / 4];   /* ROM span 0x138 */
int data_0209d4b4[8];
int sCheckpointEntranceId[8];
int sNextEntranceId[8];
int sReturnState[8];
int sCheckpointStarId[8];
/* gSceneColorFader (the COLOR fader) moved to hal/fader_wipes.cpp at gate 31:
   Scene::SetFaders dispatches two of its virtuals, so zeroed storage is a
   null vptr and the first _Z9LoadLevelijija faults on it. */
int data_020a4d84[0x368 / 4];   /* ROM span 0x368 */
/* data_02099fb0 moved to romdata (gate 35): it is file-backed arm9 data with
   the value 4 in it, and it is the COUNT func_02048720 walks when it looks
   for a free 3D voice. At 0 that loop never runs, pick stays -1, and every
   type-9 sound effect in the game comes back "no positional voice free" --
   which Sound::Play answers with a silent return. */
int data_0209d4ac[8];
int data_020a4c64[8];
int data_020a4c70[0xc0 / 4];    /* ROM span 0xc0 */
/* tier-2 state wave: DeadHit/Hurt read this arm9 bss word */
/* tier-2 round 2: arm9 bss the death/hurt/quicksand ring reads */
int sSubHandle[8];
int sTeleportDestObjs[8];
int sSubVolumeTarget[8];
/* ov100 (the message-box overlay) bss reached from St_Talk_Main; bss is
   zero at load, so host storage is the whole of it. */
int data_020a4bf8[8];
/* Sound::Play's pooled 3D voice slots, 8 bytes each. func_02048f34 clears
   SIX of them (0x020a4c18..0x020a4c48 = 0x30) and func_02048720 indexes the
   pool by data_02099fb0, so int[8] was two entries short. */
int data_020a4c18[0x30 / 4];
/* death states: KillPlayer's remaining-lives byte, and the pending-scene
   argument Scene::SetSceneToSpawn parks next to it */
int gNextSceneParam[8];
/* gate 14: the sound-group bookkeeping the kuppa tail's Sound::
   LoadGroupAndSetBank reads (dead on the port's boot, live at link time) */
int sLoadedGroup[8];
int data_0209b4a8[8];
/* gate 14 A2: the entrance step handlers' own bss, arm9 and ov002. bss is
   zero at load, so host storage is the whole of it. */
int sActorDeathTable[0xc0 / 4];   /* the per-level death table; ROM span 0xc0 */
int data_ov002_0210e14c[8];
int data_ov002_0210f350[8];
int data_ov002_0210f3b0[8];
/* data_ov089_02132880 was the same fiction; gate 22 mounts ov089 for
   real and it is the overlay's own bytes now. */
/* gate 16: the two processing-list globals the other three did not already
   need. data_020a4ba8 is the cleanup list (head, tail, callback pair);
   data_020a4b5c is the id hook _Z13func_0204302ci calls after a Process tears an
   actor down, and null is what the ROM's boot leaves it at. */
int data_020a4b5c[4];
/* gate 22: the DOOR's two. data_020a0ebc is the zero Vector3
   func_ov100_02145370 rotates the player offset around. kind:bss in config,
   so zero is what the boot leaves it at; ov089's own bss comes from the
   overlay mount. */
int data_020a0ebc[3];
/* gate 180: the QUESTION_BLOCK content body func_ov102_021492d4 reads it.
   kind:bss at 0x020a0edc, real span 8 bytes (to @16); zero is the
   boot value. */
int data_020a0edc[2];
/* gate 20: the EXIT's own scratch. _ZN11daChScene_c20LoadDestinationLevelEv stores the spawn
   record's entrance byte here on its way into _Z9LoadLevelijija; config/arm9 calls
   it kind:bss, so zero is what the boot leaves it at. */
/* gate 18 (RABBIT_KEY): _Z17StartMinigameMenuh's return-to-rec-room flag -- the
   caught chain's 8th-catch terminal writes it before the scene fade.
   kind:bss at 0x0209f298, span 4 (to sBackButtonSelected), zero is the boot value. */
unsigned char data_0209f298[4];
/* gate 27, the HUD: the star-count cache HUD::Render and RenderStarCount
   share, the red-coin counter, and the VS-mode 'results are up' flag. All
   kind:bss, so zero is the boot value. */
/* PER-PLAYER red-coin counter: ONE array on the cartridge, two names.
   src/GiveRedCoins.c writes gNumRedCoins[i] for player i; src/NumRedCoins.c
   reads gNumRedCoins and then data_0209f30d[0 .. count-2], so the DS has
   data_0209f30d == gNumRedCoins + 1. As two plain globals (30c[32] and a
   separate 30d[4]) a world counting 32 players summed 27 bytes of unrelated
   globals into the red-coin total, the coin's number popup asked for a digit
   frame that does not exist and TextureSequence's frame check stopped the
   game on every console in the room; a red coin from player i >= 1 also
   landed in a cell the sum never read. Pinned like the touch block above:
   one-byte head, the tail name owns the rest, align(1), so the linker lays
   them end to end inside the captured span. 1 + the body-slot count bytes covers
   gNumRedCoins[0 .. body-slot count] (_Z12GiveRedCoinsii up to player 31,
   Stage::InitResources' sixteen-byte clear). The same store-on-one-name /
   load-on-the-other caveat as the touch block applies: none of the readers
   and writers share a function. */
__pragma(section(".dsstate$redc0000", read, write)) __declspec(allocate(".dsstate$redc0000")) __declspec(align(1)) unsigned char gNumRedCoins[1] = { 0 };
__pragma(section(".dsstate$redc0001", read, write)) __declspec(allocate(".dsstate$redc0001")) __declspec(align(1)) unsigned char data_0209f30d[4] = { 0 };
unsigned char data_0209fc9c[4];
unsigned char sPauseState[4];
/* gate 25: the bottom screen. The three SetSubBgyOffset scroll shadows and
   SetBg0Offset's pair (the 2D layer's own copy of the BGxHOFS/VOFS words),
   the owner-language byte GetOwnerLanguage returns, the per-slot camera-
   button state Stage::CheckCameraInput latches, and the ov002 byte it sets
   when the zoom button is pressed. All kind:bss, so zero is the boot value. */
int gBg0HOffset[4];
int gBg0VOffset[4];
int gSubBg2VOffset[4];
int gSubBg2HOffset[4];
int gSubBg0HOffset[4];
int gSubBg0VOffset[4];
unsigned char data_020a0f00[4];
unsigned short data_0209f368[4];
unsigned char gZoomButtonActiveTimer[4];
/* gate 26, the boot spine: Stage::LoadModel's last line parks the Stage's own
   ModelComponents pointer (Stage+0x874) here, and CopyTexPalFromLevelModel
   reads it back. kind:bss in config, so zero until LoadModel runs. */
int data_0209f320;
/* gate 31: StartFile's last global before the scene fade. kind:bss in config,
   so zero is the boot value; the port's boot never set it because nothing
   called StartFile.

   Gate 35 also wants this one, as the id of the star just collected, and
   declared it int[8] in its own branch. FOUR BYTES IS THE RIGHT SIZE: config
   puts sMenuWaitTimer at 0x0209f22c, so the extent is 4, and 32 would have
   covered seven adjacent symbols. star_flow.cpp reads it as a single
   `extern unsigned char`, which this serves. Same shape as the data_020a0e68
   stomp: when two streams want one symbol, the symbol table decides, not
   whichever declaration is more generous. */
/* gate 31: the two SetNumPlayers seats beside data_0209fc5c (hosted in
   hal/scene_vs_menu.cpp since the width reconciliation) -- the player count
   and the per-slot controller index. kind:bss. */
unsigned char data_0209fc50[4];
char data_0209fc64[4];
/* gate 31: the second word CleanCommonModelDataArr resets. Its two siblings
   (the count at 0x0209cef8 and the array at 0x0209cefc) already have storage
   in hal/model_host.cpp; this one had no reader until the level teardown
   called the ROM's own reset. kind:bss, so zero is the boot value. */
int data_0209cef0;
/* wave 3 lane b: the slot the Scene head chain clears.
   Scene::ResetFadersAndSound (src/_ZN8dScene_c19ResetFadersAndSoundEv.cpp) writes
   `gSoftResetFader = 0` right after Scene::SetFaders installs the brightness
   fader, and Scene::BeforeBehavior (src/_ZN8dScene_c14BeforeBehaviorEv.cpp) is
   the reader: on zero it parks &gSoftResetBrightnessFader here, and _Z13func_0202345cv loads
   it back and walks it as a `void **`. So the slot holds a POINTER, and null
   is both the boot value and the value the reset writes, which is what makes
   kind:bss storage the whole of it.

   FOUR BYTES, sized by the delta-to-next-symbol rule the buffer comment
   above sets out, not by this file's generic int[8]. config/arm9/symbols.txt:

       gSoftResetPending kind:bss addr:0x0209f1e0
       gSoftResetFader kind:bss addr:0x0209f1e4
       sBootJobDone kind:bss addr:0x0209f1e8

   so the ROM extent is 0x0209f1e8 - 0x0209f1e4 = 4, and the generous default
   would have been 28 bytes of slack lying across the next seven symbols --
   the gCollectedStarId shape exactly. There is no overrun to leave room for
   either: all three touches in the tree are single-word. include/decl_common.h
   already declares it `void *`, which is what this definition is. */

/* ---- wave 4 lane d: the five globals Scene::BeforeBehavior's closure reads --
   Slot 7 of _ZTV8dScene_c (config/arm9/relocs.txt from:0x0209269c to:0x0202e3d4)
   is Scene::BeforeBehavior, and its callee closure -- func_02023544,
   _Z10Wipe_Resetv, func_0202ed14, func_0202fb30 -- reads five DS BSS symbols the
   port hosted nowhere. Every one is kind:bss in config/arm9/symbols.txt, so
   zero is the value the ROM's own boot leaves them at, EXCEPT gSoftResetBrightnessFader
   (see its own note below).

   EVERY SIZE IS THE DELTA TO THE NEXT SYMBOL, the rule the buffer comment
   further up this file sets out, not this file's generic int[8]. Read out of
   config/arm9/symbols.txt, one subtraction each:

       gSoftResetPending .. gSoftResetFader   4 bytes
       data_0209b2fc .. gMgSaveDirty   4 bytes
       gMgSaveDirty .. gMgMenuPage   4 bytes
       gSoftResetBrightnessFader .. @331  12 bytes  (0xc)
       data_0209f61c .. data_0209f648  44 bytes  (0x2c)

   THE LAST ONE IS THE INTERESTING SIZE, and it runs the opposite way from the
   data_0209d74c overrun the header describes: the generic int[8] would be too
   SMALL, not too large. src/func_0202ed14.c writes its argument's byte at
   +0x28, so the object has to be at least 0x29 bytes; 32 would have put that
   store one byte past the end and into whatever the linker parked next.
   src/func_0202fb30.c reads the same +0x28 and copies +0x18 into +0x14. 44
   covers both with the ROM's own extent and no guessing. */

/* the paused/suspended flag. src/func_02023498.c is the only writer that ever
   sets it (`gSoftResetPending = 1` on the pause edge) and that TU is not in the
   link, so on the port it stays 0 -- which is what makes the whole first
   branch of Scene::BeforeBehavior unreachable. decl_common.h does not declare
   this one; src/func_02023544.c and src/func_02023498.c both spell it
   `extern unsigned char`, which this serves. */

/* the two bytes func_02023544 clears beside it. decl_common.h:1781 already
   declares data_0209b2fc as `u8`, which this serves. */
unsigned char data_0209b2fc[4];
unsigned char gMgSaveDirty[4];

/* ---- run mg15 lane MENU: the two the minigame MENU scene reads ------------
   dScMiniGm_c (scene 5, ov005) is the first thing in the port to touch either.
   Both are kind:bss in config/arm9/symbols.txt, so zero IS the ROM's boot
   value, and both are sized by the ROM delta to the next symbol -- the rule
   this file's header sets out -- rather than by the generic int[8]:

       gMgMenuPage .. gMgMenuRow   4 bytes
       gSoftResetDisabled .. sSoftResetComboHeld   4 bytes

   gMgMenuPage is the menu's PAGE flag: _ZN11dScMiniGm_c8BehaviorEv sets it to 0 or 1
   as the two dwell counters expire, and UpdateGameSelect branches its whole
   hit test on it -- 0 is the six-cell grid (3 columns x 2 rows), 1 is the
   three-cell strip. src/_ZN11dScMiniGm_c13InitResourcesEv.cpp reads it during InitResources.
   Sits beside gMgSaveDirty and data_0209b2fc above because it IS the next
   word of that run; the ROM spacing is contiguous.

   gSoftResetDisabled is the WHOLE-GAME PAUSE-ADJACENT flag. Four arm9 TUs already
   spell it (func_02019ac4 gates its work on it, func_02030aa4 and
   _Z13func_02023498v write it), so it is not an ov005 invention -- ov005 is just
   the first spelling of it that reaches this link. _ZN11dScMiniGm_c13InitResourcesEv sets it
   to 1 at the tail of InitResources and src/MiniGm_SaveIfDirty.cpp clears it
   on the way out, which is the menu declaring itself open and then closed.
   Every TU in the tree spells it `unsigned char`, which this serves. */
unsigned char gMgMenuPage[4];

/* the record _Z10Wipe_Resetv resets: 44 bytes by the rule above, and the ROM
   writes nine of its fields through func_0202ed14 (eight fresh values plus
   +0x18 copied from +0x14). */

/* THE BRIGHTNESS FADER, and the one symbol here where zero is NOT the ROM's
   boot value. src/__sinit_02074edc.c is the ROM's static initialiser for it:
   it installs a vtable, sets currInterp to 0x1000, and registers a destructor.
   The port does not link that sinit, so this storage is zeroed BSS and its
   FIRST WORD -- the vptr -- is null.

   THAT IS SAFE HERE AND IT IS NOT LUCK, but it is a real constraint and the
   next lane through has to keep it. Only two dispatch-relevant touches of
   this object reach the binary, both from Scene::BeforeBehavior (the head
   also writes field4 and parks the address in gSoftResetFader, behind the same
   guard), and both are behind
   `gSoftResetPending != 0`, which no linked writer ever makes true (above). One of
   the two, the matched FaderBrightness::SetForwardTime, ENDS IN AN UNQUALIFIED
   IsAtEnd() -- a virtual call through this vptr -- so on a null vptr it would
   fault. See the note in hal/method_faces.cpp beside its face, and
   slice_w1l2.txt's blocked list: giving this object a real vptr is the
   ROM-class swap slice_w1l3.txt already names, not a line here.

   Twelve bytes, the ROM extent, which is also exactly the C view of the class
   in include/FaderBrightness.h (vtable 0x0, currInterp 0x4, speed 0x8). */
}
DSSTATE_END

/* Sound:: is a NAMESPACE in the TU that calls this one (YAX mangling) */
namespace Sound { void UnsetPlayerVoiceGroup(); }
/* RETIRED at ALIAS2 (wave 8, the main -> port sync): src/_ZN5Sound21UnsetPlayerVoiceGroupEv.cpp is a real definition in main tree now and emits ?UnsetPlayerVoiceGroup@Sound@@YAXXZ itself, so this EMPTY host stand-in was the second definition (LNK2005). Retiring it puts the ROM body on the call, which is what the port wants.
   The body is kept below under #if 0 rather than deleted, so the
   evidence in it stays readable. */
#if 0
void Sound::UnsetPlayerVoiceGroup() {}
#endif

DSSTATE_BEGIN
/* ---- gate 29: the particle engine's own BSS -------------------------------
   sParticleHeapCursor/7c/80 are the three cursors of the arena SysTracker::
   Initialise carves out (base, end, current) and func_02023178 bump-allocates
   from. 84/88/8c are the VRAM bases it caches off func_02045ce0/cf0/d10.
   data_020a4d30 is the engine's scratch slot.

   LCG_STATE_0204da4c is the particle RNG's state -- SPLRandom_VecFx32 multiplies
   by 0x5eedf715 and adds 0x1b0cb173 three times to pick a random emission
   direction. The additive constant means a zero seed is a perfectly good
   seed, and starting at zero is what keeps a selftest frame reproducible. */
extern "C" {
int sParticleHeapCursor[8];
int sParticleHeapEnd[8];
int sParticleHeapBase[8];
int sParticlePalVramLowCursor[8];
int sParticleTex4x4VramCursor[8];
int sParticlePalVramHighCursor[8];
int data_020a4d30[8];
int LCG_STATE_0204da4c;

/* THE TWO THE FAULT PROBE WAS ANSWERING FOR. Retiring
   port/unmatched/Player_InitResources.cpp (w6-c, HOSTABI_RETIRE_SYMS) took
   away the definition site for the level id and the local player index, and
   nothing failed to link: tests/fault_probe.h carries
   /alternatename:_gCurrentSublevel=_port_fault_no_level and the matching
   _gLocalPlayerId=_port_fault_no_pidx, weak fallbacks meant to fire only in
   the reduced probe binaries. They fired in the full walk_window instead, so
   the game would have read the level it was standing in out of a fault-probe
   placeholder.

   The alias guard could not see it: both aliases FIRED, which is the shape it
   reads as healthy, and both pairs sit in alternatename_baseline.txt as
   reviewed weak-symbol idiom. dsstate_guard is what caught it, by noticing
   two hosted DS globals had landed outside the captured section.

   Four bytes each by next-symbol delta in config/arm9/symbols.txt
   (0209f250/0209f254, 0209f2f8/0209f2fc), not the one byte the retired copy
   declared. Here rather than in hal/level_boot.cpp beside the other six of
   that retirement because level_boot re-declares gCurrentSublevel as a scalar
   `signed char` further down its own file. */
}

/* gate 50: ov080's PAINTING (daPicGate_c, 307) bss is NOT here -- it is
   mounted with the rest of the overlay in ov080_syms.txt so the DATA table
   data_ov080_0212775c's pointers into it get rebased to host addresses. See
   the header of port/ov080_syms.txt for why auto_bss would break the Shared
   FilePtr walk. */
