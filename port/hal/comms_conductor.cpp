#include "comms_seam.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
// 2. THE DS GLOBALS.
//
// SECTION SUFFIX $ymp3, and it is a measurement rather than a preference --
// the same one MP1 made for $wcomms and MP2 for $xmp2. port/tools/battery.py's
// header: some hosted DS data reaches the geometry stream as a POINTER VALUE,
// so an insertion INTERIOR to .dsstate shifts every global past it and changes
// the rendered frame while leaving the section base where it was. 'y' sorts
// after 'x' and before the $zzz sentinel, so these land above MP1's three and
// MP2's two and NOT ONE existing hosted global moves. Appending to $xmp2
// instead would leave the order within that group to the linker, and if these
// landed ahead of MP2's two then MP2's two would move.
//
// SIZED BY ROM SPAN, every one, off config/arm9/symbols.txt -- never by the
// width of the first field that reads them. That is the undersized-host-global
// trap this tree has been bitten by before, and this lane is unusually exposed
// to it because the conductor reaches most of these as interior addresses.
//
// FOUR OF THEM ARE BANDS, not arrays. A band is a run of separate DS symbols
// the game walks with base-plus-offset arithmetic, so the run has to come out
// of the linker contiguous and in ROM order or the walk lands in the wrong
// place. Each band below is declared as consecutive definitions inside one
// section group, every member sized by its own ROM span, and no member carries
// an align specifier -- an alignment request on an interior member is exactly
// what would insert the padding that breaks the walk. Only band heads are
// aligned, and only where the DS requires it.
// ===========================================================================

// ONE SECTION PER SYMBOL, NUMBERED, and that is the part that actually makes a
// band a band. MSVC merges grouped sections in lexical order of the text after
// the '$', but WITHIN one section the order of objects is left to the linker.
// So a single .dsstate$ymp3 holding eleven definitions would compile, link, and
// put them in whatever order it liked -- and a base-plus-offset walk over a
// shuffled band reads the wrong bytes while every guard stays green.
// hal/camera_bridges.cpp already solved this for the two records it hosts by
// giving each symbol its own numbered section ($camcomm0000..0007), so the
// SORT ORDER pins the layout rather than a hope about link order. Same recipe
// here, and the numbers are assigned in ROM address order.
//
// align(1), NOT camera_bridges' align(2). Its runs are all even-sized so
// align(2) packs them exactly; this band has members of 1, 3 and 0x11 bytes,
// and an alignment request of 2 on an odd-sized member is precisely what would
// insert the interior padding that breaks the walk. The precedent for a
// byte-granular ROM-spaced run is hal/auto_bss.cpp:184-189's $touch0000..0003.
// The section name is passed WHOLE, not assembled from a prefix and a suffix:
// __pragma(section(...)) takes a literal and does not perform the adjacent
// string-literal concatenation an ordinary expression would, so ".dsstate$ymp3"
// sec is a syntax error rather than a name. camera_bridges.cpp's COMM passes
// the full name for the same reason. Found by the compiler, recorded here so
// the next band does not rediscover it.
#define MP3_BSS(sec, name, size)                                     \
    __pragma(section(sec, read, write))                              \
    extern "C" __declspec(allocate(sec))                             \
    __declspec(align(1)) unsigned char name[size] = {0}

// ---------------------------------------------------------------------------
// TWO GLOBALS INHERITED FROM MP2, AND THEY KEEP $xmp2 ON PURPOSE.
//
// MP2's host-side driver hosted these and is retired by the same commit that
// adds this file. They are reproduced here VERBATIM -- same section, same
// order, same sizes, same initialisers -- rather than folded into $ymp3 below,
// because $xmp2 sorts before $ymp3 and moving them into it would shift them
// past MP1's three at $wcomms... no: it would shift them relative to nothing,
// but it WOULD change the address of every hosted global between $xmp2 and
// $ymp3, and "not one existing hosted global moves" is the property this whole
// suffix scheme exists to keep. Only one object contributes to $xmp2 either
// way, so the group's contents and position are byte-for-byte what MP2 left.
//
// data_020a0f08 is the player-count byte src/func_0203ea5c.c:288 writes from
// block+0x0D. data_02099e18 is the counter :279 bumps once per live peer record
// accepted. Both are now written by the LINKED ROM code rather than by a
// transcription, which is the difference this lane makes.
#pragma section(".dsstate$xmp2", read, write)
extern "C" {
__declspec(allocate(".dsstate$xmp2")) __declspec(align(4))
unsigned char data_020a0f08[4] = {0};   // 0x020a0f08 .. 0x020a0f0c
__declspec(allocate(".dsstate$xmp2")) __declspec(align(4))
int data_02099e18 = 0;                  // 0x02099e18 .. 0x02099e1c
}

// ---------------------------------------------------------------------------
// BAND A -- THE STAGED 0x20 BLOCK, 0x020a1020 .. 0x020a1040.
//
// This is the one the wire format is about. src/func_0203ea5c.c:169-186 stages
// into it with ten MI_CpuCopy8 moves and then passes &data_020a1020 to
// func_020406b4, which hands the transport exactly kCommsBlockBytes from that
// address. Eleven DS symbols, spans 2+1+3+2+1+1+1+2+1+1+17, and they sum to
// exactly 0x20 -- which is the arithmetic that proves the band is the block
// rather than a guess about where it ends.
//
// THE BLOCK IS A UNION AND THE SPANS SAY SO. :171 copies 0x16 bytes into
// data_020a1023, whose own span is 3: on the info-mode path the block carries a
// 0x16-byte payload at +3 and the structured fields at +6..+0x10 are simply not
// there. That write RUNS THROUGH its neighbours, which is legal on the DS
// because they are contiguous, and is the reason this cannot be eleven separate
// arrays.
MP3_BSS(".dsstate$ymp3a0000", data_020a1020, 2);    // flag word
MP3_BSS(".dsstate$ymp3a0001", data_020a1022, 1);    // info selector
MP3_BSS(".dsstate$ymp3a0002", data_020a1023, 3);    // + the 0x16 info payload, see above
MP3_BSS(".dsstate$ymp3a0003", data_020a1026, 2);    // frame counter, low half
MP3_BSS(".dsstate$ymp3a0004", data_020a1028, 1);    // stylus x
MP3_BSS(".dsstate$ymp3a0005", data_020a1029, 1);    // stylus y
MP3_BSS(".dsstate$ymp3a0006", data_020a102a, 1);    // touch
MP3_BSS(".dsstate$ymp3a0007", data_020a102b, 2);    // heading
MP3_BSS(".dsstate$ymp3a0008", data_020a102d, 1);    // player count, slot 0 only
MP3_BSS(".dsstate$ymp3a0009", data_020a102e, 1);    // reserved, slot 0 only
MP3_BSS(".dsstate$ymp3a0010", data_020a102f, 17);   // game payload
// On the DS, 0x020a1040 -- data_020a1040, the local record -- begins here.
// In the host image that symbol is hal/camera_bridges.cpp's and lands
// elsewhere, which costs nothing: every walk the conductor makes over this
// block is internal to it, and the guard checks the band's own 0x20 span
// rather than an adjacency the host does not have.

// ---------------------------------------------------------------------------
// BAND B -- THE PER-PLAYER INFO BUFFERS, 0x020a10a4 .. 0x020a1154.
//
// src/func_0203ea5c.c:171 indexes data_020a10a4 by (sel * 0x16) for sel in
// 0..3, and :319/:327 walk data_020a10ba and data_020a1112 in 0x16 strides.
// Eight DS symbols summing to 0xb0, which on the DS lands exactly on
// data_020a1154. That adjacency is a ROM fact and NOT a host one -- the four
// records are camera_bridges' and sit elsewhere in this image -- and it is not
// needed: every index above stays inside this band's own 0xb0. Its own span is
// what the guard checks. data_020a10a4's own span is 2 and
// its declared use is 0x16, which is the undersized trap in miniature: the
// other 20 bytes are data_020a10a6, and they are only there if the band is.
MP3_BSS(".dsstate$ymp3b0000", data_020a10a4, 2);
MP3_BSS(".dsstate$ymp3b0001", data_020a10a6, 20);
MP3_BSS(".dsstate$ymp3b0002", data_020a10ba, 22);
MP3_BSS(".dsstate$ymp3b0003", data_020a10d0, 22);
MP3_BSS(".dsstate$ymp3b0004", data_020a10e6, 22);
MP3_BSS(".dsstate$ymp3b0005", data_020a10fc, 2);
MP3_BSS(".dsstate$ymp3b0006", data_020a10fe, 20);
MP3_BSS(".dsstate$ymp3b0007", data_020a1112, 66);   // three more 0x16 slots

// ---------------------------------------------------------------------------
// BANDS C and D -- THE SIX-BYTE PLAYER IDS, 0x020a0fa6 and 0x020a0fbe.
//
// src/func_0203ea5c.c:321 asks func_0204271c whether an id is all zero and :326
// asks func_02042748 whether two are equal; both read THREE HALFWORDS, so the
// unit is six bytes, and the loops step by 6 four times and three times. Four
// ids of six bytes is 24, which is what the conductor's own extern declares and
// what span+neighbour gives: 18+6 and 18+8.
// Spans 18+6 and 18+28, contiguous, landing exactly on data_020a0fec.
MP3_BSS(".dsstate$ymp3c0000", data_020a0fa6, 18);
MP3_BSS(".dsstate$ymp3c0001", data_020a0fb8, 6);
MP3_BSS(".dsstate$ymp3c0002", data_020a0fbe, 18);
MP3_BSS(".dsstate$ymp3c0003", data_020a0fd0, 28);

// ---------------------------------------------------------------------------
// THE SCALARS. No interior addressing reaches any of these, so they are loose
// definitions and their order does not matter. Still ROM-spanned.
// ONLY WHAT THE LINKED TUs ACTUALLY REFERENCE. Hosting the rest of these two
// bands "while we are here" would add hundreds of bytes to the captured
// .dsstate span for no reader, and every one of them would be a symbol the
// port defines and nothing explains.
MP3_BSS(".dsstate$ymp3s0000", data_02099e1c, 4);   // "open the radio" one-shot, :137-140
MP3_BSS(".dsstate$ymp3s0001", data_0209d4fc, 4);   // the per-VBlank sleep queue
MP3_BSS(".dsstate$ymp3s0002", data_020a0ef0, 4);   // bound selector, :142-146
MP3_BSS(".dsstate$ymp3s0003", data_020a0ef8, 4);   // info-mode countdown
MP3_BSS(".dsstate$ymp3s0004", data_020a0efc, 4);   // the leave countdown, :449-457
MP3_BSS(".dsstate$ymp3s0005", data_020a0f28, 4);   // channel, folded into the flag at :149
MP3_BSS(".dsstate$ymp3s0006", data_020a0f2c, 4);   // the WM command argument, :361 and :381
// data_020a0f94, the ROM's own link-state word, MOVED to hal/comms_seam.cpp
// (run link100, lane WM1, rung W0). src/func_02040714.c is linked now and the
// seam is what publishes that word, so the seam hosts it -- and hal/comms_seam.cpp
// is what the small mp_comms_seam probe links WITHOUT this file, which is what
// turned the move from tidy into necessary. It keeps this section name
// (".dsstate$ymp3s0007"), this size and this align(1), and it is still the only
// contribution to that suffix, so its address is exactly where it was and not
// one global in the save-state bracket moved.
MP3_BSS(".dsstate$ymp3s0008", data_020a0f98, 5);   // last role, src/func_0203df40.c:77

// data_020a1fc0 is the WM work buffer. WFS_GetStatus reads word 3 of it and
// nothing else in the linked tree touches it, but it is SPANNED at 0x440 and
// goes down at 0x440: a 16-byte host sized by its one reader is precisely the
// undersized-global bug this section's banner names.
MP3_BSS(".dsstate$ymp3s0009", data_020a1fc0, 0x440);

// ---------------------------------------------------------------------------
// THE TOUCH RING, 0x020a0dd8 and 0x020a0df8.
//
// src/func_0203b9bc.c walks data_020a0df8 as NINE eight-byte entries (its index
// fixup is `if (k < 0) k += 9`) and settles the answer into data_020a0dd8. The
// spans agree exactly: on the DS 0x020a0df8 runs 0x48 = 9 * 8 up to
// data_020a0e40 (a ROM adjacency, not a host one), and
// data_020a0dd8 runs 8 up to data_020a0de0. Two independent facts meeting is
// what makes this a reading rather than a guess.
//
// THE PORT FILLS THIS RING, as of the same lane that hosted it.
// hal/sub_screen.cpp's poll_touch writes one entry per frame at the bottom of
// its store, in the ROM's own encoding, and advances the write index in the
// DS's own halfword at tpState+12 -- the one src/func_0205edc8.c returns
// and src/func_0203b9bc.c reads backwards from, so the reader and the writer
// walk one ring rather than two.
//
// It did not, for the first half of this lane, and the consequence is worth
// keeping: func_0203b9bc took its `flag == 0` branch every frame and published
// the ROM's IDLE QUAD (a = b = 0xff, c = 0, d = 0), so the stylus could not
// cross the wire while the key crossed perfectly on the same run. That
// asymmetry is what identified the missing writer. Rung 3 now carries both
// halves, 298 of 300 frames each direction.
//
// The ROM's three-consecutive-samples debounce is left alone, so a one-frame
// tap still does not cross. That is correct DS behaviour, not a limitation.
MP3_BSS(".dsstate$ymp3t0000", data_020a0dd8, 8);
MP3_BSS(".dsstate$ymp3t0001", data_020a0de0, 8);
MP3_BSS(".dsstate$ymp3t0002", data_020a0df8, 0x48);   // nine eight-byte ring entries
extern "C" int port_comms_conductor_check_layout(void) {
    // ONLY THIS FILE'S OWN BANDS, and the three cross-file rows an earlier
    // revision carried are gone. They were wrong in a way worth recording,
    // because the instinct that put them there is a reasonable one.
    //
    // On the DS band A ends exactly where data_020a1040 begins, band B ends
    // exactly where data_020a1154 begins, and the ring ends exactly where
    // data_020a0e40 begins -- so asserting those adjacencies looks like
    // asserting the layout is ROM-faithful. In the HOST IMAGE those three
    // anchors are hosted by other files (camera_bridges, auto_bss) and land
    // nowhere near this file's sections: measured, data_020a1040 sits at
    // 0004:00000002 against band A at 0004:000eb53c. Wiring those rows would
    // have failed the guard on a build that is perfectly correct.
    //
    // AND THEY DO NOT NEED TO BE ADJACENT, which is the actual point. Every
    // walk the conductor makes is INTERNAL to one band: it hands the transport
    // &data_020a1020 for kCommsBlockBytes, and band A is 0x20 bytes of its own
    // symbols; it indexes data_020a10a4 by sel*0x16 for sel 0..3 and steps
    // data_020a1112 three times by 0x16, all inside band B's 0xb0. What has to
    // be true is that each band is CONTIGUOUS AND LONG ENOUGH, which is what
    // these rows check. Where a band happens to sit is the linker's business.
    //
    // The ring needs no row at all: nothing walks from it into another symbol,
    // and its length is a compile-time property of its own declaration.
    struct { const char *what; long got, want; } rows[] = {
        // Band A -- the staged 0x20 block, checked end to end and at its two
        // interior landmarks. The span row is the one that matters: it is the
        // guarantee that &data_020a1020 really is kCommsBlockBytes of this
        // band's own storage.
        {"A: block span 1020..102f+17",
         (long)((data_020a102f + 17) - data_020a1020), 0x20},
        {"A: 1020->1023",  (long)(data_020a1023 - data_020a1020), 3},
        {"A: 1020->102f",  (long)(data_020a102f - data_020a1020), 0x0f},
        // Band B -- the per-player info buffers, four 0x16 slots then three
        // more, checked end to end and at both stride landmarks.
        {"B: band span 10a4..1112+66",
         (long)((data_020a1112 + 66) - data_020a10a4), 0xb0},
        {"B: 10a4->10ba",  (long)(data_020a10ba - data_020a10a4), 0x16},
        {"B: 10a4->1112",  (long)(data_020a1112 - data_020a10a4), 0x6e},
        // Band C -- the six-byte player ids: 0fa6 must carry a full 24 bytes
        // before 0fbe starts, which is 18 of its own plus 0fb8's 6.
        {"C: 0fa6->0fbe",  (long)(data_020a0fbe - data_020a0fa6), 0x18},
    };
    int bad = 0;
    for (unsigned i = 0; i < sizeof rows / sizeof rows[0]; ++i) {
        if (rows[i].got == rows[i].want) continue;
        ++bad;
        std::fprintf(stderr,
                     "  [conductor] BAND BROKEN %s: +%ld, expected +%ld\n",
                     rows[i].what, rows[i].got, rows[i].want);
    }
    if (bad)
        std::fprintf(stderr,
                     "  [conductor] %d band row(s) wrong. The linker did not "
                     "lay the .dsstate$ymp3 sections out in sort order, so "
                     "every base-plus-offset walk in src/func_0203ea5c.c is "
                     "reading the wrong bytes.\n", bad);
    return bad == 0;
}
namespace port {
// The two registers src/func_0203df40.c:31 reads. 0x04000130 is inside ntr's
// mapped IO window (io.cpp's kRegions, IO_BASE 0x04000000 size 0x2000) and
// 0x027FFFA8 is inside the shared region, so both are plain backing store here
// and a write is read back verbatim -- which is the whole mechanism.
enum : unsigned {
    kDsKeyInput   = 0x04000130u,
    kDsSharedPad  = 0x027FFFA8u,
    kDsKeyMask    = 0x2fffu,
};
bool comms_fanout_active() {
    // The override is parsed once; the transport half is asked every time.
    //
    // NOT because anything installs a transport mid-run. An earlier version of
    // this comment said scene_vs_menu.cpp does, and that is contradicted by a
    // measured banner already in the tree: scene_vs_menu.cpp's install is INERT
    // on every path that exists today, because main() installs 249 lines before
    // the VS section reaches it and the call finds g_installed already true.
    // Read its banner before believing any claim about when a transport arrives.
    //
    // Live because LATCHING WOULD BAKE IN THE HARNESS. Where the install
    // happens today -- main(), on every process, because the process started
    // rather than because anything asked for a session -- is a property of this
    // harness and not of the game. When ov075's lobby is driven for real, a
    // player picks VS and the transport arrives after the frame loop is already
    // turning. Asking every time costs a pointer load and survives that; a latch
    // would be correct today and wrong on the day the lobby lands, which is the
    // kind of thing nobody re-derives when it breaks.
    static int forced = -2;              // -2 unparsed, -1 absent, else 0/1
    if (forced == -2) {
        const char *s = std::getenv("SM64DS_COMMS_FANOUT");
        forced = s ? (std::atoi(s) != 0) : -1;
    }
    if (forced >= 0) return forced != 0;
    // A host-side mode may run each console as its OWN solo game: the local
    // player is driven by local input and a remote body is corrected only by
    // the aux channel, never by the input fan-out. Fanning the wire's records
    // into the pad arrays would drive the LOCAL player off the wire and stop
    // the game being solo, which is the one thing such a mode must keep. The
    // input exchange still runs (it is what keeps the aux channel connected);
    // only the record-to-pad fan-out stands down. An explicit override still
    // wins, handled above, for a proof that deliberately wants the
    // shared-input path.
    return comms_transport() != nullptr;
}
void comms_publish_pad(unsigned held) {
    // THE BAND GUARD, RUN ONCE, HERE. It has to be called from somewhere or it
    // is decoration, and this is the one entry that is reached on every frame
    // of every path that uses this file -- solo included, which matters,
    // because a shuffled band would corrupt the local record whether or not a
    // transport is installed. Reported once either way: a guard that is silent
    // on success is indistinguishable from a guard nobody called, which is the
    // failure mode this whole paragraph exists to avoid.
    static bool checked = false;
    if (!checked) {
        checked = true;
        /* run mg16 lane MP4: decide once whether the state-sync layer runs.
           HERE rather than in the frame loop, and the reason is a measurement:
           the frame-loop site sat inside `if (real_camera)` on the level path
           and never logged at all, in solo OR in a session. This one-shot is
           reached on every path that has input -- the band guard beside it
           proves so in every playlog -- and it runs after the transport
           install, which is what sync_decide needs to look at. */
        if (port_comms_conductor_check_layout())
            std::fprintf(stderr,
                         "  [conductor] bands OK -- A spans 0x20 at "
                         "&data_020a1020, B spans 0xb0 at &data_020a10a4, "
                         "C gives 0fa6 its full 24 bytes\n");
    }


    // ACTIVE LOW, which is the entire point. See THE STUCK CONTROLLER at the
    // top of this file: the ROM XORs with 0x2fff on the way in, so the register
    // has to carry the complement for the game to read what the player pressed.
    *reinterpret_cast<volatile unsigned short *>(kDsKeyInput) =
        (unsigned short)((held ^ kDsKeyMask) & kDsKeyMask);

    // Left at 0. On the DS this is the ARM7's copy of the X/Y/hinge bits, which
    // are not visible at 0x04000130; the port has no ARM7 publishing it, and 0
    // is the correct "none of those pressed" value once the OR is taken.
    *reinterpret_cast<volatile unsigned short *>(kDsSharedPad) = 0;
}
}  // namespace port
