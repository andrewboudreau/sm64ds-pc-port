// THE GAME LOOP'S INPUT RING (run linkfull, lane B5INPUT). Phase 0x17 of the
// ROM's frame loop, src/func_020197b8.c:41-46:
//
//     data_0209d50c = 0x17; func_0203bc7c();
//     v = *(unsigned short*)((char*)data_020a0e58 + (data_020a0e40 << 2));
//     data_0209d534[data_0209d4e8] = v;
//     data_0209d51c = v;
//     data_0209d4e8 = (data_0209d4e8 + 1) & 0x1f;
//
// The three ring words were hosted nowhere (0 map rows on INT53). They are .bss
// on the cartridge (config/arm9/symbols.txt), so they start at zero here too,
// and each one gets its ROM span (next symbol minus this one):
//
//   data_0209d4e8   0x0209d4e8, 4 bytes   the ring cursor, a byte (& 0x1f)
//   data_0209d51c   0x0209d51c, 4 bytes   this frame's held keys, an int
//   data_0209d534   0x0209d534, 0x40      the ring: 32 halfwords of held keys
//
// INSIDE THE .dsstate BRACKET, for rom_frame.cpp's reason: a save state taken
// mid-session has a ring position, and a restore that came back with a
// different one would have changed the ROM's state. The captured span grows by
// these words, so a disk save state written by an older build refuses to load
// (by design, the same as every hosted word before).
//
// Nothing in src/ reads the ring but _Z13func_020197b8v itself, which the port does
// not run yet (tests/walk_window.cpp's level loop stands in for it phase by
// phase and writes these at phase 0x17), so the words the cartridge keeps are
// the words the port keeps, and the day the ROM's loop drives the frame they
// are already here.
//
// AND THE LAST WORD OF PHASE 0x17 (run linkfull, lane LOOPIN2). The block ends
// with func_020197b8.c:47, _Z13func_02023498v(), the soft-reset latch, and that TU
// keeps one word nothing in the link hosted (0 map rows on RESET2's tip):
//
//   sSoftResetComboHeld   0x0209f1dc, 4 bytes   "the combo is still held": raised
//                                          with the latch and dropped only once
//                                          data_020a0e44 reads 0 again, so a
//                                          held L+R+START+SELECT cannot re-arm
//                                          the reset (src/func_02023498.c:18-22)
//
// Its two neighbours were already hosted in hal/auto_bss.cpp: gSoftResetDisabled
// (the minigame menu's hold on the reset, raised by dScMiniGm_c::InitResources)
// and gSoftResetPending (the latch itself). .bss on the cartridge, so zero here,
// and inside the bracket for the ring's reason; the captured span grows by it.
#include "dsstate_seg.h"

DSSTATE_BEGIN
extern "C" {
unsigned char data_0209d4e8[4];
int data_0209d51c;
extern "C" unsigned short data_0209d534[];   /* the key ring: func_0201a96c.cpp defines it */
unsigned char sSoftResetComboHeld[4];
}
DSSTATE_END

/* ---- THE LEVEL LOOP'S ACCOUNT OF PHASES 0x17 AND 7 (lane LOOPIN2) ----------
   Host counters, not ROM state, so they sit OUTSIDE the bracket: a save state
   captures exactly the cartridge's words above and nothing of these.

   tests/walk_window.cpp's level loop calls port_loopin2_p17 once per
   _Z13func_02023498v() at phase 0x17 and port_loopin2_p7 once per func_0201a4bc()
   at phase 7. One line at exit gives both counts beside the ROM's own phase-6
   account (hal/rom_frame.cpp's port_rom_frame(), which the frame cross-check
   holds to the host frame), so "phase 7 runs once per frame" is a measurement
   in every run. And one line per EDGE of the soft-reset words -- the latch
   (gSoftResetPending), the combo-held word (sSoftResetComboHeld) and the reset fade that
   dScene_c::BeforeBehavior parks in gSoftResetFader -- says whether the combo
   reached the ROM's writer and what the ROM did next. Silent on every frame
   nothing changes, which is every frame of ordinary play. */
#include <cstdio>
#include <cstdlib>

extern "C" {
extern unsigned char gSoftResetPending[4];   /* hal/auto_bss.cpp: the latch */
extern void *gSoftResetFader;              /* hal/auto_bss.cpp: the reset fade */
int port_rom_frame(void);                /* hal/rom_frame.cpp */
/* hal/method_faces.cpp: the FaderBrightness faces' entries while the latch is
   up and the receiver is the reset fader gSoftResetBrightnessFader (its own banner). */
void port_fader_latch_face_hits(unsigned *set_forward, unsigned *is_at_end);
}

static unsigned g_l2p17_calls, g_l2p17_rises, g_l2p17_zeroed, g_l2p7_calls;
static int g_l2_armed;

static void loopin2_report(void)
{
    unsigned sf = 0, ae = 0;
    port_fader_latch_face_hits(&sf, &ae);
    std::fprintf(stderr,
                 "[loopin2] level loop: phase 0x17 _Z13func_02023498v %u call(s), "
                 "phase 7 func_0201a4bc %u call(s), %d frame(s) of the ROM's "
                 "phase-6 account; soft-reset latch rose %u time(s), PadData / "
                 "TouchInfo / Ctrl zeroed on %u frame(s) (func_0203bc50, "
                 "func_0203bb14, _Z10ResetInputv); dScene_c::BeforeBehavior's latch "
                 "branch: FaderBrightness::SetForwardTime %u, IsAtEnd %u\n",
                 g_l2p17_calls, g_l2p7_calls, port_rom_frame(), g_l2p17_rises,
                 g_l2p17_zeroed, sf, ae);
    std::fflush(stderr);
}

static void loopin2_arm(void)
{
    if (g_l2_armed) return;
    g_l2_armed = 1;
    std::atexit(loopin2_report);
}

/* latch_before: gSoftResetPending as _Z13func_02023498v found it, which is exactly the
   test its first branch makes, so a non-zero value means that call zeroed
   PadData, TouchInfo and the Ctrl records. held_before / held_after: the local
   player's held word either side of the call. */
extern "C" void port_loopin2_p17(int frame, int latch_before,
                                 unsigned held_before, unsigned held_after)
{
    loopin2_arm();
    ++g_l2p17_calls;
    if (latch_before) ++g_l2p17_zeroed;
    if (!latch_before && gSoftResetPending[0]) ++g_l2p17_rises;
    static unsigned was;
    /* sSoftResetComboHeld is not printed: the game keeps its own file-private
       copy (src/unnamed/arm9/0202/func_0202345c.cpp), so the word defined
       above is never written and would always read 0 (catchup1 twin scan). */
    const unsigned now = (unsigned)gSoftResetPending[0] |
                         (gSoftResetFader ? 0x10000u : 0u);
    if (now == was && !latch_before) return;
    was = now;
    std::fprintf(stderr,
                 "[loopin2] f%d soft-reset words after _Z13func_02023498v: "
                 "gSoftResetPending=%u fade=%p | held %04x -> %04x%s\n",
                 frame, (unsigned)gSoftResetPending[0],
                 gSoftResetFader, held_before & 0xffffu, held_after & 0xffffu,
                 latch_before ? " (the latch was up: PadData, TouchInfo and the "
                                "Ctrl records zeroed)" : "");
    std::fflush(stderr);
}

extern "C" void port_loopin2_p7(void)
{
    loopin2_arm();
    ++g_l2p7_calls;
}

/* catchup1 round 3: src/.../func_0201a96c.cpp defines the held-key ring. The C name reaches that storage: */
#pragma comment(linker, "/alternatename:_data_0209d534=?data_0209d534@@3PAGA")
