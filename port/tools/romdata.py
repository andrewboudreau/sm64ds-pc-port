#!/usr/bin/env python3
"""Emit the ROM data constants the gate-4b slice references.

These are read-only tables in the arm9 image (sine table, identity matrices,
texture-matrix constants). They are Nintendo's bytes, so they are never
committed: this reads the local decompressed arm9 (extracted/arm9_dec.bin,
flat image based at 0x02000000) and writes a C file under the build tree.
"""

import pathlib
import struct
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import link_only  # noqa: E402
import romblob_common  # noqa: E402
import decomp_root  # noqa: E402

# extracted/arm9_dec.bin: the DECOMPRESSED arm9, flat at base 0x02004000
# (verified by two code anchors: MI_Copy36B @0x0205a548, DecompressLZ16
# @0x0205a754). extracted/dsd/arm9/arm9.bin is the COMPRESSED payload
# (arm9.yaml: compressed true) and must not be read directly. Addresses at
# or past bss_start are runtime-initialized and belong in the HAL.
#
# BSS_START WAS 0x02094640 AND THAT WAS TOO LOW BY 0x69c0, which quietly
# pushed a whole run of file-backed .data into the HAL as zeroed storage.
# The ROM settles it twice over: extracted/dsd/arm9/arm9.yaml records
# bss_start 34162240 = 0x0209b000, and config/arm9/delinks.txt lays the
# sections out as .data 0x02086bc0..0x0209b000, .bss 0x0209b000..0x020aa420.
# The image itself reaches 0x020a0f78, so every address below 0x0209b000 has
# real bytes behind it. Two of the symbols the old boundary excluded had
# already been reconstructed by hand in port/hal/model_host.cpp, and the ROM
# bytes agree with both reconstructions exactly -- which is the check that
# the new boundary is the right one.
BASE = 0x02004000
BSS_START = 0x0209B000

# (symbol address, byte length, C element type). BSS addresses do not belong
# here -- they are runtime-initialized and live in the HAL as storage.
#
# THE ELEMENT TYPE IS LOAD-BEARING and was wrong for the two OBJ size tables
# until 2026-08-05. The emitter below only knew two widths, 4 for "int" and 2
# for everything else, so "char" read the twelve-byte tables as SIX 16-BIT
# values and declared them `char[6]`. Both the count and the values came out
# wrong: MSVC truncated each halfword to its low byte, so the width table read
# {8, 32, 16, 32, 8, 16} where the ROM has {8,16,32,64, 16,32,32,64, 8,8,16,32}.
# OAM::GetObjWidth indexes it at `size + shape * 4`, up to 15, so every sprite
# past shape 1 size 1 also read off the end of a six-byte array.
#
# The two are 16 rather than 12 for the same reason the ROM gets away with 12:
# 0x020755ac is 0x020755a0 + 12, so the shape-3 row of the width table IS the
# first row of the height table. Emitting 16 bytes each keeps every index the
# code can form inside its own object and reading exactly the bytes the DS
# reads. Shape 3 is the prohibited OBJ shape and no sprite uses it.
TABLES = [
    (0x02082128, 48, "int"),        # Model ctor default Matrix4x3
    (0x02082190, 48, "int"),        # matrix Render uses for a NULL argument
    (0x02082214, 0x4000, "short"),  # s16 trig table the material bind indexes
    (0x020755A0, 16, "unsigned char"),  # OBJ width table  (size + shape * 4)
    (0x020755AC, 16, "unsigned char"),  # OBJ height table
    # ONE ROW THAT DELIBERATELY OVERLAPS THE TRIG TABLE, run mg11 lane SNW.
    # 0x02082314 is 0x100 bytes into the s16 table above -- entry 128 -- and
    # dScMgSnowball_c reads it BY NAME rather than by index:
    # src/Snowball_UpdateCamera.c does fdiv(0xc0000, data_02082314), and the ROM
    # at 0x02126958 is `ldrsh r1,[r1]` off a pool word holding 0x02082314, so
    # the read is one SIGNED HALFWORD and include/decl_common.h declares it
    # `extern short data_02082314;` to match.
    #
    # THE OVERLAP IS THE ESTABLISHED SHAPE IN THIS LIST, not a new liberty:
    # the two OBJ tables above overlap by four bytes and the header explains
    # why. A separate two-byte emission is right here for the same reason it
    # is there -- both hosts hold the ROM's own bytes for their own address,
    # nothing writes either (the trig table is read-only), and the alternative
    # would be a hand-typed constant in a HAL file, which is a value nobody
    # can re-derive from the image.
    (0x02082314, 2, "short"),       # trig table entry 128, read by name
]


# Initialized arm9 data referenced by name: address + size resolve from
# config/arm9/symbols.txt (size = delta to the next symbol, the ovdata.py
# convention). Emitted as aligned byte arrays so any struct view works.
NAMED = [
    # Run mg15 lane MENU: the minigame SELECTION menu's two row words. Both are
    # arm9 .data (config/arm9/delinks.txt: .data 0x02086bc0..0x0209b000), four
    # bytes each by the delta rule, and NEITHER carries a relocation -- checked
    # over both spans in config/arm9/relocs.txt, no `from:0x0208a170` and no
    # `from:0x0208a174` line exists. Their ROM values are 0x00000003 and
    # 0xffffffff, read out of extracted/arm9_dec.bin at 0x0208a170.
    #
    # THE -1 IS THE WHOLE REASON THEY COME FROM HERE AND NOT FROM A HAL LINE.
    # gMgLastSelected is the menu's SELECTION and its boot value is the "nothing
    # picked" sentinel; src/func_ov005_020c0378.cpp only ever writes a row index
    # into it, so a host that zeroed it would boot the menu with row 0 already
    # chosen and nothing in the game would ever say so. gMgGroupBase is the
    # highlighted row base, advanced by _ZN11dScMiniGm_c15UpdateGroupTabsEv, and 3 is where the
    # ROM starts it. The port does not write Nintendo's numbers down; it reads
    # them, which is the same argument every other row in this list makes.
    #
    # ONE HONEST GAP, NAMED RATHER THAN LEFT TO BE FOUND. gMgLastSelected is
    # MUTABLE DS state -- the menu writes the player's selection into it -- and
    # nothing this emitter produces lands in .dsstate. Every symbol in this
    # file goes into romdata.c's own section (measured: walk_window.map puts
    # _gMgGroupBase at 0003:00004660, outside the captured span), which is the
    # mechanism's existing behaviour for all 30-odd NAMED entries and not
    # something these two introduced. The consequence is bounded and specific:
    # a save state taken while the minigame menu is open does not roll back the
    # highlighted row or the selection. Moving the emitter's output into
    # .dsstate is a change to every row here and is not this lane's to make.
    "gMgGroupBase", "gMgLastSelected",
    "data_0208a178", "data_0208c178",
    # Run link100 wave 14 (lane SEAT14D): the OBJ char blob the debug level
    # select loads, `GX::LoadOBJ(data_0208c378, 0, 0x2000)` in
    # src/_ZN10dScTitle_c13InitResourcesEv.cpp. It is the exact sibling of the
    # BG char blob one row up: 0x0208c378 to the next config symbol
    # (data_0208e378) is 0x2000, which is the length the call itself reads, it
    # sits below BSS_START so real bytes are behind it, and config/arm9/
    # relocs.txt has ZERO relocations from anywhere inside the span. Hosting it
    # as zeroed HAL storage instead would have been a fabrication: the scene
    # would load 8 KB of blank tiles and look like it worked.
    "data_0208c378",
    "data_0208e504", "data_0208e538", "data_0208e548", "data_0208e54c",
    "data_0208e55c", "data_0208e56c", "data_0208e57c", "data_0208e58c",
    "data_0208e59c", "data_0208e5b0", "data_0208e5c0", "data_0208e5d4",
    "data_0208e5e4", "data_0208e5f0", "data_0208e5fc", "data_0208e608",
    "data_0208e610", "data_0208e624", "data_0208e634", "data_0208e648",
    "gCurrentAreaId",
    # run linkw wave 5 (lane w5-c): the two identity-quaternion constants
    # (x,y,z,w) = (0,0,0,0x1000) the restored TILTING_PLATFORM_BFS chain
    # reads -- Quaternion_FromVector3's fallback (data_02075b04) and the
    # tilting Behavior's slerp rest pose (data_02092768). Both 0x10, both
    # reloc-free (checked config/arm9/relocs.txt over the full spans).
    # Lane w5-b reached the same pair from level 13: the WorkElevator
    # rider-push chain reads them too -- func_ov021_02112024 resets its
    # orientation from data_02092768 and Quaternion_FromVector3 falls
    # back to data_02075b04.
    "data_02075b04", "data_02092768",
    # Run link60 lane NFS: the NitroSDK archive NAME. FSi_InitRom passes
    # &data_0209a048 and a length of 3 to FS_RegisterArchiveName, which packs the three
    # characters into the archive record's first word and compares against it
    # on every "arch:" prefixed path. It is four bytes of arm9 rodata ("rom\0",
    # the next symbol is 0x0209a04c), and it comes from here rather than being
    # spelled as a C literal for the reason every other row here exists: the
    # port does not write Nintendo's bytes down, it reads them.
    "data_0209a048",
    # Run link100 lane SND2, rung R4: the SDAT's own path,
    # "/data/sound_data.sdat". _Z13func_020133bcv loads its address and hands
    # it to func_02050f34, which walks it through the ROM's own directory
    # table. Twenty-two bytes of arm9 .data including the terminator.
    #
    # THE SIZE IS PINNED AT 0x18 AND THE DEFAULT WOULD BE WRONG. The next
    # symbol in config/arm9/symbols.txt is _ZTV12ActorDerived at
    # 0x0208e4b8, so the delta rule would emit 0x20 -- and the last word
    # of that span, 0x0208e4b4, carries a RELOCATION
    # (config/arm9/relocs.txt: "from:0x0208e4b4 kind:load
    # to:0x02086e78"). It is the vtable's own RTTI-pointer prefix word,
    # not part of the string, and emitting it would write a DS address
    # down as a host constant. 0x18 covers the string, its terminator and
    # the padding, and stops before both prefix words. The scout's report
    # says 0x20; this is the correction.
    "data_0208e498:0x18",
    # Run link100 lane FRAME: the EIGHT arm9 .data rows _ZTV5Stage slot 6's
    # closure reads. Seating the ROM's own Stage::Behavior brings the pause
    # screen, its five menu arms and the Message::Display* family into the link
    # (port/slice_gate220.txt), and those bodies index six ROM tables and read
    # two ROM scalars that no host file had. Every one is checked the way lane
    # STAGE checked its three: relocation-free over its WHOLE span
    # (config/arm9/relocs.txt has no `from:` row inside any of the eight) and
    # sized by delta-to-next-symbol in config/arm9/symbols.txt, which is exact
    # on all eight.
    #
    # THE FOUR 0x020755xx ROWS ARE TILE-OFFSET TABLES, NOT POINTERS, and that
    # is the thing to check before reading the `PAG` in ?data_020755c0@@3PAGA
    # as an address the port would have to map. The callers are
    #   src/_ZN5Stage20PS_UpdateOptionsMenuEv.cpp:36
    #       `scr = G2S::GetBG1ScrPtr() + data_020755c0[j];`
    #   ..:47 the same shape on data_020755c4
    #   src/_ZN5Stage25PS_UpdateOkAndBackButtonsEb.cpp:67 on data_020755cc
    #   src/_ZN5Stage17PS_UpdateSaveMenuEb.cpp:28 on data_020755c8
    # -- the POINTER is the accessor's return value and the table holds u16
    # tile INDICES added to it. So there is nothing to relocate and nothing to
    # map, which is also why the relocation check comes back clean.
    #
    #   data_02075258   12 bytes  the eleven sound ids _Z13func_02011c54v walks,
    #                             one per iteration into func_0204f9c4
    #   data_020755c0    4        options-menu row offsets
    #   data_020755c4    4        options-menu row offsets, second table
    #   data_020755c8    4        save-menu row offsets
    #   data_020755cc    8        OK/Back button offsets
    #   data_02075610   16        the per-button run lengths
    #                             PS_UpdateOkAndBackButtons:68 counts to
    #   data_020756d0   32        the pause-menu message ids
    #                             PS_Update:204 hands Message::Display
    #   data_0208ee40    4        the scalar the level-clear arm reads
    "data_02075258", "data_020755c0", "data_020755c4", "data_020755c8",
    "data_020755cc", "data_02075610", "data_020756d0", "data_0208ee40",
    # Run mg11 lane TTI: the one s16 dScMgTrampoline_c's InitResources divides
    # by. src/minigames/d_s_mg_trampoline.cpp reads it as `(s32)data_02082414` and the
    # ROM does `ldrsh r1,[r1]` at 0x02122210 with the pool word 0x02082414,
    # feeding cstd::fdiv(0xc0000, that) -- the scene's field of view. It is an
    # INTERIOR ADDRESS of the trig SPAN already hosted above
    # ((0x02082214, 0x4000, "short")), 0x200 bytes in, and /alternatename
    # cannot express an offset from a symbol, so the name is hosted here in its
    # own right. The duplication is safe and cannot drift: the span is a
    # read-only table (this file's own opening line says so) and both copies
    # come from the same extracted image on every build. ROM span 0x200, the
    # delta to the next config symbol data_02082614.
    "data_02082414",
    # Run link60 lane FDR2: the two BG palette words the dWipe_c time setters
    # push through GX/GXS::LoadBGPltt. _ZN7dWipe_c14SetForwardTimeEjj loads data_020926c8 and
    # _ZN7dWipe_c15SetBackwardTimeEjj loads data_020926cc, each with a length of 2, on the
    # `type != 2` branch; the type == 2 branch uses their bss counterparts
    # data_0209f600 / data_0209f604, which are runtime storage and are hosted
    # in port/hal/fdr_arm9_fader_seat.cpp instead. Four bytes each by ROM span
    # (the next symbols are 0x020926cc and 0x020926d0) and reloc-free: the
    # nearest `from:` rows in config/arm9/relocs.txt are 0x020926c4 below and
    # 0x020926dc above, so nothing inside either span relocates. They are the
    # cartridge's own colours and are read here rather than written down.
    "data_020926c8", "data_020926cc",
    # run mg11 lane TTE: the fov divisor dScMgTrampoline2_c's and
    # dScMgTrampoline_c's InitResources read. Both do
    # `fdiv(0xc0000, (s32)data_02082414)` and the ROM is `ldrsh r1,[r1]` on that
    # exact address, so it is a single signed halfword and decl_common.h:1662
    # declares it `extern s16 data_02082414;`. It is an INTERIOR ADDRESS of the
    # trig table TABLES already emits at 0x02082214 (0x4000 bytes), 0x100
    # halfwords in; the span to the next symbol is 0x200 and nothing inside it
    # relocates (config/arm9/relocs.txt has no `from:` row in 0x02082414..
    # 0x02082614). Emitting it by name duplicates 0x200 read-only bytes that
    # already sit inside the trig table object, which is the cost of giving the
    # decomp's own symbol name a definition; the alternative was a synthesised
    # host global with no ROM provenance, which this file exists to avoid.
    #
    # THE ROW ITSELF IS ABOVE, NOT HERE. Two mg11 lanes needed this symbol and
    # each added it with its own derivation; the merge keeps ONE row and BOTH
    # notes, because the two lanes checked different things (that one reasons
    # from the /alternatename limitation, this one from relocs.txt) and neither
    # reading is redundant. A second entry would emit the 0x200 bytes twice.
    "data_02082178",
    # GetActorDebugName reads ACTOR_DEBUG_NAMES[i] for every i < 0x188, so the
    # table is 0x188 words (0x02090e80..0x020914a0). config cuts it at the page
    # boundary 0x02091000 (data_02091000: no relocation targets it, its words
    # are the same string pointers), and the delta rule hosted only the first
    # 0x60 rows; ids 0x60.. read past the array. Found by the catchup1 Stage D
    # romtable sweep. A pin rather than a CONTIG run because nothing names the
    # cut, so one array is the whole object (the page-boundary note in
    # named_entries).
    "ACTOR_DEBUG_NAMES:0x620", "data_020914a0",
    "STAGE_DEBUG_NAMES", "data_02092654", "data_02092668", "data_0208e500", "data_02086a58", "gSubSoundID", "data_02086b58",
    "gMusicTrackModeOverride", "sMusicTrackMode", "sLayer1Music", "sLayer2Music",
    "sLayer3Music", "gLastCourseSublevel", "data_02086384", "sPlayerSeqLimits", "sCutsceneEndWaving", "sCutsceneIntroPeachLetter", "gCheckpointSublevel", "gNextSublevel", "data_0208eecc", "data_02092134",
    # sDanceCutsceneTable WAS HERE and was six RELOCATED words: the Kuppa/star-get
    # script-pointer table _ZN11KuppaScript18StartDanceCutsceneEi indexes as sDanceCutsceneTable[arg0] and
    # _ZN11KuppaScript3RunEPh runs. Byte-copied it held six raw DS addresses, so
    # _ZN11KuppaScript7ProcessEv faulted walking a garbage pointer. Hosted with real
    # host pointers to byte-copied script blobs in port/hal/ptr_tables.cpp
    # (table 3); putting the name back here would define the symbol twice and,
    # if the byte copy won, restore the crash.
    # ---- run link100, lane SND1, rung R10: THE UNDERWATER MUSIC FILTER'S
    # FIVE RAMP CONSTANTS, and the dB table rung R8 reaches beside them.
    #
    # src/func_020494cc.c programs a two-channel ramp table from these five
    # words -- the per-frame step _Z13func_020490b0P12NNSSndHandle adds to the filter's current
    # value, sign-flipped for the falling arms -- and hal/sdat/consumer.cpp
    # skipped the whole three-part mechanism for exactly one reason: "not one
    # of those five is defined anywhere in the port, so it would not link".
    #
    # ALL FIVE ARE arm9 .data, four bytes each by the delta rule (the next
    # symbol is four bytes on in every case), and RELOC-FREE: config/arm9/
    # relocs.txt has no `from:` row anywhere in 0x020821f0..0x02082214, so a
    # byte copy carries no DS address. The five `to:` rows that DO name these
    # addresses are the loads in func_020494cc itself, which is the reference
    # into them, not a relocation inside them.
    #
    # AND THEY ARE NOT ZERO, which is why they are read rather than written
    # down: 0x1451, 0x0a28, 0x1451, 0x1451, 0xcb33 out of extracted/arm9_dec.bin.
    # A zeroed stand-in would make _Z13func_020490b0P12NNSSndHandle's `a > 0 / a < 0` test take its
    # `continue` arm on every frame and the filter would be a subsystem that
    # runs and does nothing -- the exact shape a seat is supposed to avoid.
    "data_02082200", "data_02082204", "data_02082208", "data_0208220c",
    "data_02082210",
    # data_02086484, 0x2d4 = 724 bytes by the delta rule (the next symbol is
    # fsi_default_command), and the span is likewise reloc-free. It is the ROM's own
    # attenuation-to-volume curve and src/func_0205b63c.c indexes it as
    # data_02086484[x + 0x2d3] over x in [-0x2d3, 0] -- 724 entries exactly, so
    # the ROM's own code states the length. The bytes run 0x00 at the bottom to
    # 0x7f at the top, monotone, which is what a -72.3 dB .. 0 dB curve looks
    # like. Its sibling data_02086384 (the 128-entry s16 table) has been in this
    # list for as long as the sequencer has; this is the byte-wide one the
    # streamed-sound path uses.
    "data_02086484",
    "sMusicVolumeTarget",
    # ---- run link100, lane R2BD: THE DOWNLOAD-PLAY ADVERTISEMENT ISLAND ----
    #
    # rung R2b of lane BOOTSCOUT's boot plan. _Z13func_0201fec8v -- main's own
    # _Z13func_0201a054v calls it -- hands func_0203db64 two arm9 .rodata blobs, and
    # port/slice_r2abc.txt refused the rung because none of them was hosted.
    # This is that island, and it CLOSES: fourteen symbols, 864 bytes, twenty
    # relocated words, and not one relocation anywhere below the first level.
    #
    # WHAT IT IS, read out of extracted/arm9_dec.bin rather than guessed. The
    # five 0x1c blobs are the DS Download Play game-info records, one per
    # firmware language, and each is
    #
    #     +0x00  0                  +0x04  the UTF-16 game NAME
    #     +0x08  the UTF-16 game DESCRIPTION, this record's language
    #     +0x0c  the icon GRAPHICS path     +0x10  the icon PALETTE path
    #     +0x14  0x00800000                 +0x18  4
    #
    # data_0208f278 is "Super Mario 64 DS"; data_02075530 is "Scramble to get
    # the Stars!\nThe caps will help you out."; data_0208f29c and data_0208f2d0
    # are "/data/2D_cad/d_2d_MARIO64DS_IPL_download_ncg.bin" and "..._ncl.bin".
    # The sixth blob, data_02075358, is the WM PARENT PARAMETER block, and it
    # corroborates hal/wm_arm7.cpp:98-106 exactly: +0x32 is 0x200 and +0x34 is
    # 0x20, which is where that file's parentMaxSize/childMaxSize come from.
    #
    # THE RELOCATION AUDIT, run over config/arm9/relocs.txt for all fourteen
    # spans (lane R2BD):
    #
    #     data_020752cc +0x04 -> 0x0208f278   +0x08 -> 0x02075398
    #                   +0x0c -> 0x0208f29c   +0x10 -> 0x0208f2d0
    #     data_020752e8 the same four, +0x08 -> 0x020753f8
    #     data_02075304 the same four, +0x08 -> 0x02075530
    #     data_02075320 the same four, +0x08 -> 0x020754c4
    #     data_0207533c the same four, +0x08 -> 0x0207545c
    #     data_02075358, data_02075398, data_020753f8, data_0207545c,
    #     data_020754c4, data_02075530, data_0208f278, data_0208f29c,
    #     data_0208f2d0                       ZERO relocations each
    #
    # SO THE TWENTY WORDS ARE NOT BYTE-COPIED INTO THE GAME. This file's own
    # rule -- a byte-copied relocated word carries a DS address into the host,
    # which is what killed sDanceCutsceneTable -- holds here too, and the remedy is
    # the tree's: the bytes come from the image, and the twenty words are
    # REBOUND to the host addresses of the eight objects above before any ROM
    # code reads them. The binder is port_r2b_bind_mbinfo() in
    # hal/boot_arms.cpp; it checks each word still holds the DS address the
    # relocation names before it overwrites it, so a drifted image refuses
    # rather than binds. The binder lives beside the arm that needs it instead
    # of in hal/ptr_tables.cpp because this island has ONE reader, at one point
    # in the boot, and that file is not this lane's.
    #
    # Sizes are the delta-to-next-symbol default and every one is exact:
    # 0x1c x5, then 0x40, 0x60, 0x64, 0x68, 0x6c, 0x70, and 0x24, 0x34, 0x34.
    "data_020752cc", "data_020752e8", "data_02075304", "data_02075320",
    "data_0207533c", "data_02075358",
    "data_02075398", "data_020753f8", "data_0207545c", "data_020754c4",
    "data_02075530",
    "data_0208f278", "data_0208f29c", "data_0208f2d0",
# ---- run lvled, lane intro-cutscene: the NEW-FILE OPENING's script data ----
#
# src/__sinit_02073e6c.c is what makes the opening's FOUR-script chain work:
# the chain arguments and the argument-blob pointers are stored as ZERO on
# disc and it writes them at boot, byte by byte because the fields are
# unaligned. On the host it writes HOST addresses, which is exactly why it
# has to run rather than the pointers being byte-copied.
#
# EVERY SYMBOL BELOW WAS AUDITED INDIVIDUALLY against config/arm9/relocs.txt
# before being put here, because this list BYTE-COPIES and a byte-copied
# relocated word carries a DS address into the host -- the sDanceCutsceneTable
# crash this file's own header banks. The audit is
# out/intro-cutscene/reloc_audit.txt; the closure is 155 symbols and it
# TERMINATES (level 2 is 54 clean, 1 record, 1 further target).
#
# The 96 names here have ZERO relocated words in their spans, with ONE
# reasoned exception:
#
#   sCutsceneIntroPeachMarioAppears (the third script, the one carrying the closing
#   LoadLevelNoReturn) is flagged with six relocated words. All six are
#   FALSE POSITIVES and none is dereferenced. dsd's arm9 relocations are
#   INFERRED -- arm9 loads at a fixed address, so a data word whose value
#   looks like an address gets marked. Walking the script record by record
#   out of arm9_dec.bin (_ZN11KuppaScript7ProcessEv's own rule: s[0] is the record
#   length, +1 cmd, +2 beg, +4 end, +6 args) puts all six at ARGUMENT
#   offsets inside cmd 0x00/0x01/0x02 records -- the player-movement
#   commands, which _ZN11KuppaScript16RunPlayerCommandEPhii consumes as VALUES. The values are
#   0x020b0000, 0x020a0244, 0x02091000, 0x02090800, 0x020f0000 and
#   0x020f1000: position coordinates that happen to look like DS
#   addresses. The only kuppa command that dereferences an argument as a
#   pointer is cmd 6 (the chain), and its arg is ZERO on disc and written
#   by the sinit. Decode in out/intro-cutscene/script_decode.txt.
#
# The 55 POINTER RECORDS that really do carry pointers are NOT here. They
# are hosted with real host pointers in hal/intro_script_records.cpp, the
# way table 2 of hal/ptr_tables.cpp hosts the 39 camera-script records.
#
# NAMES THE AUDIT PRODUCED THAT ARE DELIBERATELY NOT BELOW:
#   sCutsceneCreditsBoB, sCutsceneGrandStar, sCutsceneEndWaving, sCutsceneIntroPeachLetter, sCutsceneEndingMarioFall
#     already carried by this file above; a second entry is a redefinition.
#   @259, gKuppaScriptFader
#     NOT romdata. Both live in the 0x0209xxxx working region and their
#     spans come out NEGATIVE here (the next symbol is below them).
#     gKuppaScriptFader is the FaderBrightness OBJECT _ZN11KuppaScript7ProcessEv drives
#     through SetToStart/SetForwardTime -- live state hosted as BSS, so
#     byte-copying it here would be a category error.
    "sCutsceneIntroEnd", "sCutsceneEndingStartMusic", "sCutsceneEndingMarioLandHold", "sCutsceneGrandStarFly",
    "sEndingMarioToPeachFocusPoints", "sTHIWigglerCreditsSplineFocusPoints", "sEndingMarioToPeachPosPoints", "sTHIWigglerCreditsSplinePositionsPoints",
    "sCutsceneCreditsCCMOutside", "sCutsceneCreditsDDDSub", "sCutsceneCreditsCotMC", "sCutsceneCreditsSA",
    "sCutsceneCreditsRR", "sCutsceneCreditsTTC", "sCutsceneCreditsTHIHuge", "sCutsceneCreditsTTM",
    "sCutsceneCreditsWDW", "sCutsceneCreditsSL", "sCutsceneCreditsDDD", "sCutsceneCreditsSSL",
    "sCutsceneCreditsVolcano", "sCutsceneCreditsTHIWiggler", "sCutsceneCreditsHMC", "sCutsceneCreditsBBH",
    "sCutsceneCreditsJRB", "sCutsceneCreditsWF", "sCutsceneCreditsCCMSlide", "sCCMOutsideCreditsSplineFocusPoints",
    "sBBHCreditsSplinePositionsPoints", "sCCMOutsideCreditsSplinePositionsPoints", "sSLCreditsSplineFocusPoints", "sSLCreditsSplinePositionsPoints",
    "sWDWCreditsSplineFocusPoints", "sWDWCreditsSplinePositionsPoints", "sTTCCreditsSplineFocusPoints", "sTTCCreditsSplinePositionsPoints",
    "sRRCreditsSplineFocusPoints", "sRRCreditsSplinePositionsPoints", "sEndingLookAtSkyFocusPoints", "sBBHCreditsSplineFocusPoints",
    "sCutsceneEndingMarioLandCloseup", "sDDDSubCreditsSplineFocusPoints", "sJRBCreditsSplinePositionsPoints", "sCCMSlideCreditsSplineFocusPoints",
    "sDDDSubCreditsSplinePositionsPoints", "sCCMSlideCreditsSplinePositionsPoints", "sJRBCreditsSplineFocusPoints", "sBoBCreditsSplineFocusPoints",
    "sSACreditsSplineFocusPoints", "sSACreditsSplinePositionsPoints", "sCotMCCreditsSplineFocusPoints", "sBoBCreditsSplinePositionsPoints",
    "sEndingLookUpAtCastlePoints", "sWFCreditsSplineFocusPoints", "sCotMCCreditsSplinePositionsPoints", "sWFCreditsSplinePositionsPoints",
    "sCutsceneEndingMarioLand", "sVolcanoCreditsSplineFocusPoints", "sVolcanoCreditsSplinePositionsPoints", "sSSLCreditsSplineFocusPoints",
    "sSSLCreditsSplinePositionsPoints", "sTTMCreditsSplineFocusPoints", "sTTMCreditsSplinePositionsPoints", "gEndWavingFocusPoints",
    "gEndWavingPosPoints", "sHMCCreditsSplineFocusPoints", "sHMCCreditsSplinePositionsPoints", "sDDDCreditsSplineFocusPoints",
    "sDDDCreditsSplinePositionsPoints", "sTHIHugeCreditsSplineFocusPoints", "sTHIHugeCreditsSplinePositionsPoints", "sEndingFlyToWindowFocusPoints",
    "sEndingFlyToWindowPosPoints", "sCutsceneEndingStarsFreePeach", "sCutsceneEndingDialog", "sCutsceneIntroPeachFlyToPipe",
    "sCutsceneEndingMarioToPeach", "sEndingPeachDescentCamPosPoints", "sCutsceneEndingPeachDescends", "sIntroStartToPipeFocusPoints",
    "sIntroStartToPipePositionPoints", "sCutsceneEndingKiss", "sIntroPipeToRoofFocusPoints", "sIntroPipeToRoofPositionPoints",
    "gIntroLakituStartToPipeOffsetFromCameraPoints", "gIntroLakituStartToPipeFocusPoints", "sCutsceneEndingPeachWakeup", "sCutsceneIntroPeachMarioAppears",
    
    # FORTY NAMES CAME OUT OF THE LINE ABOVE ON 2026-08-05, and every one of
    # them was a called-through pointer table rather than data.
    # port/tools/ptr_audit.py found them by intersecting this list with
    # config/arm9/relocs.txt: fsi_default_command is nine backup-media command
    # handlers that src/func_0205c5e4.c indexes and calls, and the 39 records
    # from data_02086bc0 to data_02086e38 are the kuppa script's camera
    # commands, copied into sScriptOps$2629 and dispatched by _Z16RunCameraCommandP9dCamera_cPhii.
    # Emitted here they were the ROM's own bytes -- forty-eight DS code
    # addresses -- which is the shape that killed a play session when the
    # billboard's two tables had it (see the note by data_02099fc4 below).
    # They are hosted with real host function pointers in
    # port/hal/ptr_tables.cpp; putting any of the names back on this list would
    # define the symbol twice and, if the byte copy won, restore the crash.
    #
    # ...and three plain constants the newly-linked handlers read, which had
    # never been referenced before for the same reason. Two camera-space offset
    # vectors, {0, 150, -600} and {600, 0, -400} in Fix12i, and the two ASCII
    # bytes FSi_GetPathCommand copies into its header buffer. No relocations in any
    # of them; they really are data.
    "data_02086ec0", "data_02086ed8", "data_0209a044",
    # gate 89: a 779-byte arm9 .rodata table func_ov002_020e88a8 (a PowerStar
    # state helper) reads a s16 out of at +0x56. No relocations in it -- pure
    # data, byte-hosted the same as every table above.
    "data_02082714",
    # run mg11 lane BNP: the 0x100 bytes at 0x02082614, which
    # src/_ZN11dScMgJump_c13InitResourcesEv.cpp (dScMgJump_c::InitResources, id 0x174) reads
    # ONE s16 out of, as the divisor of cstd::fdiv(0xc0000, data_02082614) --
    # the projection constant it parks in the scene's two camera records. It is
    # an INTERIOR address of the TABLES entry above (FX_SinCosTable_, the s16 trig
    # table), exactly as data_02082714 already is, so this row duplicates those
    # bytes into a second host array rather than aliasing into the first. That
    # is correct for a scalar read and would NOT be correct for anything doing
    # pointer arithmetic between the two symbols; nothing in the tree does.
    # No relocations land in the span (config/arm9/relocs.txt), so it is data.
    # run mg11 lane BNT: dScMgJump2_c's InitResources (_ZN12dScMgJump2_c13InitResourcesEv)
    # loads 0x02082614 and reads a SIGNED HALFWORD out of it at 0x020ef8c8
    # (ldrsh r1,[r1]) before calling func_02053258. 0x100 bytes, the delta to
    # data_02082714 above it, and no relocations in the span -- pure data,
    # byte-hosted the same as every table here.
    "data_02082614",
    # gate 32: the coin tables Actor::GivePlayerCoins indexes (id and amount),
    # and the per-character cap-model constant data_0208a0e0 sits beside them.
    "data_02075230", "data_02075238",
    # run link100, lane RENDER9 (_ZTV5Stage slot 9): the four bytes
    # Stage::RenderNumber indexes to turn a digit into an OAM tile row. Checked
    # the way lane STAGE and lane FRAME checked theirs -- config/arm9/symbols
    # .txt puts data_020755bc at the next address, so the delta is exactly 4,
    # and config/arm9/relocs.txt has no `from:` inside the span, so it is data
    # rather than a pointer table. It sits four rows below the two OBJ
    # width/height tables in TABLES above, which are the same kind of thing
    # read by the same subsystem.
    "data_020755b8",
    "data_0208ee44",
    # The three MSG_GEN_TEXT_FUNCS embedded-text data tables (_Z13func_0201aca4v /
    # _ZN7Message11AddCharPairEv read them on a 0xfe message escape): data_0208ee64 is the
    # {100,10,1} decimal place-value divisors, data_0208ee6c the button-glyph
    # font pairs, data_0208ee74 the 0..9 digit glyph indices. Pure data (no
    # relocs in the range); zeroed storage would have printed all zeros.
    "data_0208ee64",
    "data_0208ee6c",
    "data_0208ee74",
    "data_0208ee8c",
    "data_0208eeac",
    "data_0208f074",
    "data_0208f174",
    # THE PARTICLE DEFINITIONS, and the whole answer to "where do particle
    # effects come from" -- not a file, not a narc, a blob compiled into the
    # binary. Particle::SysTracker::Initialise memcmps its first 4 bytes
    # against the 8-byte magic at data_0208f668; on a mismatch it treats the
    # blob as LZ16 and decompresses it onto the heap. func_0204a17c then
    # parses it into per-effect records and builds each one's Behavior[]
    # array out of six flag bits.
    #
    # THE SIZE IS PINNED, and it has to be. The blob is an uncompressed SPA
    # archive whose own header says how big it is: " APS10_1" magic, at +0x08 a
    # u16 emitter count (321) and a u16 texture count (46), at +0x10 the size of
    # the emitter block, at +0x14 the size of the texture block and at +0x18
    # where the texture block starts. Emitters begin at 0x20, so +0x18 is just
    # 0x20 + emitters and the archive really ends at +0x18 plus +0x14: 0xc214,
    # which is exactly where the next real symbol (IDENTITY_MATRIX4X3) begins.
    #
    # The config splits the span with nine boundary symbols (data_02078000,
    # data_02080000 and friends) that are page-aligned, referenced by nothing
    # and emitted by nothing, so the default "delta to the next symbol" rule
    # handed the parser the first 0x20ec bytes, 17% of the archive.
    #
    # A SHORT BLOB DOES NOT FAIL LOUDLY, which is what makes it worth this
    # comment. The header still parses, so the subsystem still reports its 321
    # effects and allocates a record for every one of them; the ones whose data
    # fell past the cut just come out as records of nothing. An all-zero record
    # has rate 0, and func_0204c584 emits floor((rate + accumulator) / 0x1000)
    # particles with nothing else feeding that sum, so those effects stay
    # permanently silent while their systems live, move and tick exactly as
    # they should. Mario's running and sliding dust is id 0xda and landed in
    # the truncated tail. Cutting inside the TEXTURE block instead is the loud
    # failure: Particle::Texture::AllocTexVram reads a garbage length and
    # Model::GetVramOffset takes its Crash() branch during Stage::InitResources.
    "data_02075f14:0xc214",
    "data_0208f668",
    # One 8-byte constant the particle render family reads (func_0204b028,
    # func_0204b244) while building the camera-facing quad.
    #
    # data_02099fb4 and data_02099fbc USED TO BE HERE and were not constants at
    # all: each is two DS code addresses, indexed by the billboard's axis and
    # flag fields and called through. Emitted as ROM bytes they were a direct
    # jump into unmapped host memory, which is what crashed a play session on
    # the Bob-omb warp. They are hosted with real function pointers in
    # hal/particle_vtable.cpp now; putting either name back here would define
    # the symbol twice and, if it won, restore the crash.
    "data_02099fc4",
    # cstd::atan2's lookup table: atan(i/1024) in binangs, i = 0..0x400, so
    # 0..0x2000 (45 degrees) and the quadrant fixups do the rest. EVERY
    # heading in the game runs through it -- Vec3_HorzAngle, _Z14Vec3_VertAnglePK7Vector3S1_,
    # the camera's own rotation, the walk's slope math. Zeroed storage in the
    # HAL made atan2 answer 0 for every off-axis direction.
    "gArctanTable",
    # ---- VS wiring lane (ov075, scene 6) ----------------------------------
    # Five OAM/layout records the VS menu's .data table at 0x0211c968 points
    # at (patched to host addresses in hal/scene_vs_menu.cpp), and the five
    # LZ-compressed sub-screen tile blobs data_0208a0e4 indexes by language.
    # All ten are plain arm9 .data with NO relocation inside any span
    # (checked over config/arm9/relocs.txt by span), so ROM bytes are the
    # right storage; the two tables that DO carry relocations (0x0208a0e4
    # itself, and the typeinfo records) are hosted with real host pointers in
    # hal/scene_vs_menu.cpp instead -- the data_02099fb4 lesson. Plus
    # data_0208f274, one relocation-free word _ZN10dScEntry_c16UpdatePlayerSyncEv reads.
    "data_02098be8",
    "data_02098c98",
    "data_02098dd8",
    "data_02098ef8",
    "data_020990f8",
    "data_02096f08",
    "data_02097230",
    "data_020975d4",
    "data_02097950",
    "data_02097ca4",
    "data_0208f274",
    # the five per-language VS TEXT BANKS Message::LoadTextVS points
    # data_0209fd14 at (reloc-free spans; the pointer itself is set by the
    # matched TU at run time, so nothing here bakes a DS address)
    "data_020937bc",
    "data_0209325c",
    "data_02093d7c",
    "data_0209289c",
    "data_02092d3c",
    "data_02092808",
    "data_02092810",
    "data_02092824",
    # VS menu sub-screen: three palette/tile id runs func_0200f13c walks, and
    # the five OamAttr lists data_0208a0f8 points at (that table is hosted
    # with host pointers in hal/scene_vs_menu.cpp). All reloc-free by span.
    "data_0208a120",
    "data_0208a148",
    "data_0208a15c",
    "data_02098b58",
    "data_02098d20",
    "data_02098e60",
    "data_02098fa0",
    "data_02099068",
    "data_02075b14",
    "data_02075d14",
    "data_0209283c",
    "data_0209285c",
    "data_0209287c",
    # the four records data_020927a0 points at (table hosted with host
    # pointers in hal/scene_vs_menu.cpp; records are reloc-free)
    "data_02092780",
    "data_02092788",
    "data_02092790",
    "data_02092798",
    "data_0208a10c",
    "data_0208a134",
    "data_02092818",
    "data_020756b0",   # D-pad direction -> binang table (Stage::CheckInput)
    # CheckInput's three BUTTON REMAP MAPS: 16 u16 rows each, raw pad bit ->
    # Ctrl held/pressed mask (mode 0 is level play: A->1 punch, B->2 jump,
    # R->0x400 crouch, Y->0x800 run, L->0x4000 snap-behind). data_0209214c
    # below is the per-mode POINTER table over these and its words are DS
    # addresses (the relocated-pointer-table class), so the level boot
    # repoints entries 0..2 at these hosted copies rather than dereferencing
    # it -- tests/walk_window.cpp, the input staging block.
    "data_02075650", "data_02075670", "data_02075690",
    # Per-character voice-id offset. Sound::PlayCharVoice is
    # Play(1, baseId + data_02075250[ch], v), and the bytes read 00/40/80/c0
    # -- the VOICE SEQARC is laid out in 0x40-entry blocks, one per
    # character, so zeroed storage would give every character Mario's voice.
    "data_02075250",
    # _Z13SetSoundRangei's 9 (near, far) distance-limit pairs for 3D sound
    "data_020821c0",
    "data_0209214c",   # per-mode button remap pointer table (CheckInput)
    # tier-2 state wave: fader/level/message tables
    "data_02086f20",
    # PORTSYNC3: GX.cpp (main's folded GX TU) reads these four ROM tables.
    "data_02086314",
    "data_02086324",
    "data_02086326",
    "data_02086328",
    "sSavedMusicVolume",
    "data_0208eeec",
    "data_0208eeee",
    "data_02086f14",
    "data_02086f2c",
    # death states: _Z12SetNextLeveli's sublevel -> (next level, entrance) table
    "data_02075638",
    "gNextSceneId",   # Scene::SetSceneToSpawn's pending-scene ID
    # run link60 (lane S1, the scene boot): the three arm9 .data words the
    # STAR SELECT reads that no level path ever did. All three are ROM bytes
    # and none is safe as zeroed storage: data_0208ee00 is the "3D engines are
    # on" latch Enable3dEngines sets, and the two 16-byte records are the
    # OAM/G2 tables FS_LoadArchive's VRAM map walks.
    "data_0208ee00",
    "data_0208eb8c",
    "data_0208eb9c",
    # Run link100 lane CARDFS2: the archive-mount table's TWENTY-SIX string
    # symbols -- the thirteen three-character short names each data_0208ecf4
    # entry carries at +0x0c, and the thirteen NitroFS paths it carries at
    # +0x10. Lane CARDFS hosted that table with both words NULL and wrote down
    # the consequence in hal/card_mount.cpp: while they are null the ROM's own
    # mount branch must never run, because its first act is to open a null path.
    # These rows are what retire that sentence, and they arrive exactly the way
    # their two immediate neighbours above (data_0208eb8c, data_0208eb9c) do --
    # as ROM bytes this generator reads out of the image.
    #
    # THE TWENTY-SIX ARE NOT A LIST ANYONE WROTE DOWN. They are the addresses
    # the mount table's own +0x0c and +0x10 words hold, read out of
    # extracted/arm9_dec.bin at 0x0208ecf4: entry 0 names 0x0208eb5c ("ar0")
    # and 0x0208ecb0 ("/ARCHIVE/arc0.narc"), and the emitter below resolves
    # each entry's two pointers back to these symbols BY ADDRESS rather than by
    # a hand-kept order (the two orders disagree -- entry 0's short name is the
    # SECOND name in the run, and the paths are not in entry order either).
    #
    # The short names are the contiguous four-byte run 0x0208eb58..0x0208eb8c.
    # The paths are thirteen of the fourteen 0x14-byte rows from 0x0208ebac up:
    # data_0208ec74 is NOT one of them and stays out -- it is the crash string
    # src/func_02018dc4.c hands func_02018e68 when an open fails, and no entry
    # points at it.
    #
    # All twenty-six are relocation-free over their whole span, the check every
    # row in this list passes: zero `from:` rows in config/arm9/relocs.txt land
    # inside any of the 26 spans.
    "data_0208eb58", "data_0208eb5c", "data_0208eb60", "data_0208eb64",
    "data_0208eb68", "data_0208eb6c", "data_0208eb70", "data_0208eb74",
    "data_0208eb78", "data_0208eb7c", "data_0208eb80", "data_0208eb84",
    "data_0208eb88",
    "data_0208ebac", "data_0208ebc0", "data_0208ebd4", "data_0208ebe8",
    "data_0208ebfc", "data_0208ec10", "data_0208ec24", "data_0208ec38",
    "data_0208ec4c", "data_0208ec60", "data_0208ec88", "data_0208ec9c",
    "data_0208ecb0",
    # gate 31, the level handoff. Two 0xd0-byte tables, 52 entries each --
    # the ROM's whole level count. data_02092208 is the LVL_Overlay address
    # per level (Stage::InitResources indexes it); data_020758c8 the overlay
    # id that carries it (LoadLevelOverlays indexes it, and it holds level+8
    # for every one of the 52). Nintendo's own answer to "what is level N",
    # so the port asks it rather than keeping a list beside it.
    "data_02092208",
    "data_020758c8",
    # _Z12SetNextLeveli's return-to-level latch. The ROM ships it -1 ("no recorded
    # return point"), and the port had it as zeroed host storage -- so the
    # first _Z9ExitLevelv() read 0 and asked for level 0 instead of the castle
    # grounds. Its two neighbours 0x02092118/0x02092120 were already here.
    "gReturnSublevel",
    "data_0208a0e0",   # the controller count SetNumPlayers writes
    "sCutsceneDanceRotate",   # _ZN11KuppaScript28IsDanceRotateCutsceneRunningEv's demo/cutscene flag block (NoControl)
    "data_020755bc",   # character -> wipe type (_Z22StartExitCharacterWipev)
    # gate 13, the real Camera: the constant vectors the follow chain reads.
    # The mode-preset table itself is a CONTIG run, not a NAMED entry.
    "CAM_SPACE_CAM_POS_ASR_3",   # camera-space eye offset, Behavior's weather
    "data_02086efc",   # up vector {0, 0x1000, 0}    -- Render's LookAt_
    "data_02086f08",   # {0, 0, -0x1000}, the degenerate-look fallback
    "data_02086f38",   # per-level XZ bounds, low
    "data_02086f48",   # per-level XZ bounds, high
    "data_02086cc8",
    "data_02086d50",
    "data_02086e84",
    "data_02086f58",
    "data_02086e90", "data_02086e9c", "data_02086ea8", "data_02086eb4",
    "data_020874f4",   # first record past the mode table
    # gate 14, the level boot: the two kuppa scripts
    # _ZN11KuppaScript19ContinueIfNecessaryEv compares the pending pointer against.
    "sCutsceneCreditsBoB", "sCutsceneEndingMarioFall",
    # run linkw wave 2 / lane w2-e: a third kuppa script, the one
    # _ZN11KuppaScript19StartEndingCutsceneEv hands straight to _ZN11KuppaScript3RunEPh. Same shape as the two
    # above -- an opcode stream that runs out into zero padding well inside
    # its 0x50 extent, so the next-symbol delta is the whole script and no
    # size pin is needed. Mounted because _ZN11KuppaScript19StartEndingCutsceneEv is the last
    # unresolved link on ov002's real St_EndingFly_Main chain
    # (_ZN7daPly_c17St_EndingFly_MainEv -> func_ov002_020c3bdc -> _ZN11KuppaScript19StartEndingCutsceneEv).
    "sCutsceneGrandStar",
    # the Camera's SpawnInfo, for the entrance path's actor registry
    "g_profile_CAMERA",
    # the STAGE's, for the boot spine: the ActorBase constructor reads the
    # record's +4/+6 halfwords back as the two processing-list priorities, so
    # the Stage cannot be built without it (port/hal/stage_bridges.cpp).
    "g_profile_STAGE",
    # sublevel -> level-part table (_Z12GetLevelParti, the death-table index)
    "data_02075264",
    # The model-walk constants the old BSS boundary hid. data_02099f88 is the
    # DEFAULT SCALE VECTOR {0x1000, 0x1000, 0x1000} _ZN15ModelComponents13func_02044534EiP9Matrix4x3P7Vector3 copies when
    # Render is given no scale argument; with it zeroed the billboard part
    # walk builds an all-zero 3x3 and every vertex of every billboard model
    # lands on the model's translation point.
    "data_02099f80", "data_02099f84",   # BCA keyframe weight tables
    "data_02099f88",                    # billboard/part-walk default scale
    "data_02099f94",                    # texture-matrix size table
    # THE STAGE'S OWN RENDER SCALE. Stage::RenderModel ends in
    # Model::Render(&data_020755d4) and Stage::RenderModelTransparent in the
    # same call through slot 5, so this Vector3 is the only scale the level
    # model is ever drawn with. {125.0, 125.0, 125.0} in Fix12i.
    "data_020755d4",
    # skyboxID -> vrbox file HANDLE, u16[11] plus padding to the next symbol.
    # Stage::LoadSkybox indexes it at skyboxID - 1 (GetSkyboxID's 0 means "no
    # skybox" and returns early); entry 0 is 2040 = data/vrbox/vr01.bmd, the
    # castle grounds sky.
    "data_02075620",
    # gate 35, the course loop.
    #
    # Actor::GivePlayerCoins' two parallel tables, indexed by coin KIND
    # (0 yellow, 1 red, 2 blue): the coin VALUE and the id of the little
    # count-popup actor the collect spawns. Both are what turns "an actor was
    # touched" into a number on the HUD, so a zeroed pair is a coin worth
    # nothing.
    # u16[3] popup actor ids
    # u32[3] coin values
    # The per-sublevel SOUND row: three bytes each, {group, bank, bgm}, which
    # config names as three separate one-byte symbols (data_02075768/69/6a)
    # because Stage::InitResources reads the three columns with a stride of 3.
    # The size has to be spelled: the delta-to-next-symbol rule would make the
    # first column FOUR BYTES and cut off every level past the first.
    # 0x02075804 is the next real symbol, so the run is 0x9c -- 0x34 rows.
    "data_02075768:0x9c",
    # SUBLEVEL -> COURSE. Every star and every coin record is filed under the
    # answer, so zeroed storage credits all 52 sublevels to course 0. It was
    # also DECLARED two different ways -- decl_common.h has it as `signed
    # char[]`, which is what SublevelToLevel indexes, while the HAL defined it
    # as `short[64]`, so even a filled table would have been read at half
    # stride. The bytes settle it: [1] = 29, the castle grounds.
    "SUBLEVEL_LEVEL_TABLE",
    # THE 3D SOUND DISTANCE LIMITS, and the reason no positional sound effect
    # has ever played on the port. func_02048a1c is the gate every
    # Sound::Play with a Vector3 goes through:
    #
    #     lim = data_02099fac;  ...  if (lim < distance) return 0;
    #
    # and Sound::Play's caller-side response to that 0 is a silent return, so
    # a zeroed limit culls every 3D sound in the game without a word. The ROM
    # byte is 0x226 (550 units). All three are file-backed arm9 data below
    # bss_start; they were sitting in the HAL as zeroed BSS.
    # run link60 Stage 4 (lane SD0): Stage::InitResources' own ROM data.
    #
    # THE THREE FADER-WIPE FILE-ID TABLES. InitResources picks exactly one of
    # them and hands its seven entries to FaderWipe::LoadAndSetFile:
    #   data_02075600  VS mode with a level-specific archive loaded
    #   data_020755f0  sublevel 5
    #   data_020755e0  everything else, which is every level the port boots
    # Each is 0x10 by the delta-to-next-symbol rule and the loop reads seven
    # u16 out of it, so the extra halfword is padding the ROM also carries.
    # Zero relocations in any of the three spans (checked over
    # config/arm9/relocs.txt): these really are numbers.
    "data_020755e0", "data_020755f0", "data_02075600",
    # The word InitResources zeroes on its way out (its last statement block)
    # and Stage::Behavior reads. 0x4, no relocations.
    "data_02092778",
    # THE OTHER TWO COLUMNS OF THE PER-SUBLEVEL SOUND ROW, and read the
    # data_02075768 comment above this one first -- these three entries
    # deliberately emit THREE OVERLAPPING COPIES of the same ROM run.
    #
    # The run is 0x02075768..0x02075804, 0x34 rows of {group, bank, bgm}, and
    # the config names each COLUMN as its own one-byte symbol. Three different
    # readers index three different columns at stride 3:
    #   data_02075768[level*3]   _Z15GetSoundGroupIDii, already emitted at 0x9c
    #   data_02075769[level*3]   Stage::InitResources, the SEQARC bank
    #   data_0207576a[level*3]   Stage::InitResources, the layer-1 BGM
    # Byte-copied from its own start address, each array reads its own column
    # correctly, so the overlap is not a redundancy to be cleaned up -- it is
    # what makes three symbols that alias one ROM run work as three host
    # arrays. Nothing does pointer arithmetic ACROSS the three names (checked:
    # hal/star_flow.cpp walks all three columns off the single data_02075768
    # array, row[0]/row[1]/row[2], which is inside one object), so no reader
    # can observe that the copies are separate objects on the host.
    #
    # BOTH SIZES ARE PINNED AND THE PIN IS THE POINT. The next symbol after
    # data_02075769 is data_0207576a ONE BYTE later, so delta-to-next-symbol
    # hands it a one-byte array while the ROM code indexes it to level 0x33 --
    # the undersized-host-global class exactly. 0x9a is 0x02075804 minus
    # 0x0207576a, which covers the last index either column can form
    # (0x33 * 3 = 0x99).
    "data_02075769:0x9a", "data_0207576a:0x9a",
    "data_02099fa4", "data_02099fa8", "data_02099fac",
    # ...and the other half of the same silence: data_02099fb0 is the COUNT
    # func_02048720 walks looking for a free type-9 (positional) voice. The
    # ROM byte is 4. At 0 the search loop never runs, `pick` stays -1, and
    # every positional sound effect comes back "no voice free". func_02048e54
    # and func_02048e94 also write 4 into it at runtime, and neither is on any
    # path the port takes.
    "data_02099fb0",
    # run link100, lane STAGE: the OBJECT-OVERLAY tables, so the ROM's own
    # LoadOrUnloadObjectOverlays can run on the port's two hosted leaves instead
    # of a host body standing in for it. The two leaves are not the same kind of
    # thing and this comment used to imply they were (corrected by lane
    # STAGEFIX): LoadOverlay is an empty PORT_HOST_ABI body in
    # hal/scene_boot.cpp, while UnloadOverlay is a loud-once refusal seam at
    # hal/scene_vs_menu.cpp:992 that prints to stderr on its first call.
    #
    #   data_02075998[level][i]  the seven per-level SELECTOR columns
    #   data_02075804[i][v]      the overlay id each selector picks
    #   data_02092130            the resident level overlay's id, the word
    #                            LoadLevelOverlays latches and
    #                            UnloadLevelOverlays reads back
    #
    # All three are ROM BYTES and the delta-to-next-symbol rule is exact on
    # every one of them, which is the test that keeps them out of the
    # undersized-host-global class two rows above: config/arm9/symbols.txt puts
    # data_020758c8 0xc4 after data_02075804 (7 rows of 7 words, the shape
    # `data_02075804[i].t[v]` walks) and data_02075b04 0x16c after
    # data_02075998 (52 levels of 7 bytes, the ROM's whole level count), and
    # data_02092134 is 4 bytes after data_02092130. config/arm9/relocs.txt has
    # ZERO rows anywhere in 0x02075804..0x02075a40, so none of this is a
    # pointer table and ptr_audit has nothing to say about it.
    "data_02075804", "data_02075998", "data_02092130",
    # ---- run link100 lane SCENE: the two ARM9-RESIDENT SCENE CLASSES -------
    #
    # dScBoot_c (actor id 0) and dScMB_c (id 360) are the ROM's two boot
    # scenes -- Scene::PrepareToSpawnBoot picks one or the other -- and unlike
    # every scene the port had seated before them they live in arm9, not in an
    # overlay, so their artwork is arm9 .data and there is no ovdata.py mount
    # to carry it. These seven spans are every blob the two classes' own
    # bodies name; hal/scene_link100_boot.cpp and hal/scene_link100_mb.cpp are
    # the seats.
    #
    # EACH SPAN WAS AUDITED AGAINST config/arm9/relocs.txt individually, the
    # rule this list's own header states, and all seven are file-backed (every
    # address is below the 0x0209b000 BSS boundary pinned above).
    #
    #   data_020914e0  the boot scene's BG palette. GX::LoadBGPltt reads 0x40
    #                  bytes of it (src/_ZN9BootScene13InitResourcesEv.cpp) and GXS::LoadBGPltt
    #                  reads 2 and 0x40. PINNED at 0x40 rather than taking the
    #                  delta-to-next-symbol default of 0x48: the last word of
    #                  that delta is 0x02091524, the RELOCATED type_info
    #                  pointer of the vtable at 0x02091528 (relocs.txt:
    #                  from:0x02091524 to:0x020914c8), which belongs to the
    #                  vtable object and not to the palette. This is the pin
    #                  the "name:0xSIZE" form exists for.
    #   data_02091570  0x168, the sub-screen BG2 screen map, LZ77 (its first
    #                  word is 0x00080010: type 0x10, 0x800 decompressed).
    #                  Reloc-free over the whole span.
    #   data_020916d8  0x1ec, the main BG0 screen map, LZ77 0x10 -> 0x800.
    #                  Reloc-free.
    #   data_020918c4  0x84c, the shared BG character data, LZ77 0x10 ->
    #                  0x2000. ONE `from:` row lands inside it
    #                  (from:0x02091ae4 to:0x0212ffff module:overlay(85)) and
    #                  it is a FALSE POSITIVE of the class this list already
    #                  documents for LoadLevelNoReturn's six: the span is an
    #                  LZ77 stream, the "destination" is one byte short of a
    #                  64K boundary in an overlay this blob has nothing to do
    #                  with, and nothing dereferences the word -- its only
    #                  reader is DecompressLZ16 walking the stream.
    #   data_0209444c  0x20, the multiboot scene's first OBJ palette (GX and
    #                  GXS both load 0x20 at offset 0). Reloc-free.
    #   data_0209446c  0x164, the multiboot scene's OBJ character data, LZ77
    #                  0x10 -> 0x540. Reloc-free.
    #   data_020945d0  0x20, its second OBJ palette (GX::LoadOBJPltt at 0xa0).
    #                  Reloc-free.
    #
    # The five per-language multiboot images src/_ZN7dScMB_c8BehaviorEv.cpp decompresses
    # are NOT here: data_0208a0e4 and its five blobs are already hosted for
    # the VS menu (hal/scene_vs_menu.cpp and this list), and the multiboot
    # scene reads that same table.
    "data_020914e0:0x40", "data_02091570", "data_020916d8", "data_020918c4",
    "data_0209444c", "data_0209446c", "data_020945d0",
    # AND ONE FOUR-BYTE SENTINEL, here for the reason gMgLastSelected is here.
    # data_0208ee4c is "which scene overlay is resident", and its ROM value is
    # -1 (0xffffffff, read at 0x0208ee4c - 0x02004000), the "none" case. It is
    # read and written by src/func_0201a614.c and src/func_0201a694.c -- the
    # scene loader's unload-before-load discipline -- and BOTH bodies branch on
    # `!= -1` before calling the unload. A host that zeroed it would make the
    # first pass unload overlay id 0 instead of skipping, which is not a
    # cosmetic difference and nothing in the game would say so. Reloc-free
    # (config/arm9/relocs.txt has no `from:0x0208ee4c` row), four bytes by the
    # delta rule.
    "data_0208ee4c",
    # catchup1 lane ROMDATA: the arm9 .data hal/boot_globals.cpp used to carry
    # as hand-copied literals. Every one is file-backed (below BSS_START), every
    # span is reloc-free (checked over config/arm9/relocs.txt), and each literal
    # equalled the image at its own address with its length equal to the delta
    # rule's span, so the delta rule below emits the same object and bytes.
    # data_0208ee4c above is the sixth of the boot_globals set. The two overlay
    # digest symbols are a CONTIG run (tag ovdigest), not rows here.
    "data_020867bc",   # card device table, 4 rows of 0x28 (func_02060398)
    "data_02086880",   # sixteen spaces, the padding func_0206c9f4 writes
    "data_020868a0", "data_020868d4",   # MSL assert / warning formats
    "data_020868fc", "data_0208690c", "data_0208692c", "data_0208696c",
    "data_0208698c", "data_020869bc", "data_020869dc", "data_02086a08",
    "data_02086a30",   # the overlay-table assert strings
    "data_0208ecc4",   # the text myFS_OpenFileDirect (myFS_ReadFile's fault text)
    "data_0208ee50",   # ds mario, the save record tag ReadDataFromCart reads
    "data_0208ee60",   # the argument func_0201a490 passes data_0209a03c
    "data_020994d4",   # the path /BUILDTIME (PathPtr)
    "fsi_def_digest_key", "fsi_digest_key_len",   # FS_Overlay's HMAC key
    "data_0209a054",   # func_0205e3d4's 16-byte key
    # The rest of the hand-carried arm9 .data, same checks, same lane.
    "OSi_IrqCallbackInfoIndex",   # IRQ bit per DMA/timer callback (OS_IrqTable)
    "VS_STAR_SPAWN_ORDERS",       # 6 rows of 12, the VS star order (level_boot)
    "FX_AtanTable_",              # FX_Atan2's table (was in hal/scene_boot.cpp)
    "random_next",                # MSL rand's seed, ROM value 1 (ditto)
    "data_02075244",              # WDW's three water heights (GetWaterHeightWDW)
    # The 38 camera-state pairs __sinit_d_camera.cpp copies into the nineteen
    # State objects (hal/ctor_runner.cpp declares them; it carried them as
    # literals until lane ROMDATA). Each is {code word, zero this-delta}: the
    # code word IS relocated (relocs.txt has a kind:load row at each pair's
    # first word) and is kept as the raw DS address on purpose, because
    # hal/camera_states.cpp translates that address to a host call. Spans are
    # all 8 by the delta rule.
    "data_02086bc8", "data_02086bd0", "data_02086be0", "data_02086be8",
    "data_02086bf0", "data_02086bf8", "data_02086c00", "data_02086c08",
    "data_02086c10", "data_02086c18", "data_02086c28", "data_02086c30",
    "data_02086c40", "data_02086c48", "data_02086c58", "data_02086c68",
    "data_02086c70", "data_02086c98", "data_02086ce0", "data_02086d10",
    "data_02086d28", "data_02086d30", "data_02086d48", "data_02086d58",
    "data_02086d90", "data_02086da8", "data_02086db0", "data_02086db8",
    "data_02086dc8", "data_02086dd0", "data_02086de0", "data_02086de8",
    "data_02086df8", "data_02086e00", "data_02086e10", "data_02086e18",
    "data_02086e28", "data_02086e40",
    # catchup1 lane PROVE5: two more WM-chain objects hal/wm_thread.cpp and
    # hal/wm_arm7.cpp carried as hand-copied literals (the tag tool's scan did
    # not see their macro-declared initialisers). Both file-backed, reloc-free
    # (no `from:` row in config/arm9/relocs.txt inside either span), spans by
    # the delta rule.
    "data_02099e24",             # the WFS force-wakeup message (0x48)
    "SDK_OVERLAYTABLE_DIGEST",   # FS_Overlay's overlay-table digest (20)
]

# HOST OBJECTS WHOSE FIRST VALUE IS AN ARM9 SPAN. An object the port hosts on
# its own (a port-only name, or a DS .bss object the port gives a pre-boot
# value), filled with the bytes of an arm9 .data span. Before lane ROMDATA both
# were hand-typed literals in ntr/backup.cpp.
#   data_port_backup_device  row 1 of the card device table data_020867bc
#       (+0x28, the 64 Kbit EEPROM row the game names by 0xd01), kept as a
#       SEPARATE object because the port parks a pointer to it in data_020a8760
#       at static-init time and hal/boot_arms.cpp tells the two apart.
#   data_020a4b40  the save record tag. On the DS it is .bss and
#       _Z13func_02042f68iPh copies data_0208ee50 (ds mario, eight bytes, no
#       zero among them) into it at boot; the port also gives it that value up
#       front, so a boot that skips the copy (SM64DS_ROM_MAIN=0) still has it.
#   (name, arm9 address, size)
HOST_ROWS = [
    ("data_port_backup_device", 0x020867E4, 0x28),
    ("data_020a4b40", 0x0208EE50, 8),
]

# ARM7 PROGRAM CONSTANTS. extracted/arm7.bin is the ARM7 program exactly as the
# cartridge stores it (uncompressed; ROM header +0x30 offset, +0x3c size). The
# port's host mixer stands in for the ARM7 sound driver, and the one table it
# needs from that driver lives here, in no arm9 or overlay image. Offsets are
# into the file, not addresses: part of the program is autoloaded elsewhere.
#   (name, offset into arm7.bin, size)
ARM7_ROWS = [
    # SND's attack table: 19 bytes, the attack rate for attack values
    # 0x6d..0x7f (hal/sdat/mixer.cpp cnv_attack indexes it at 0x7f - a).
    ("port_arm7_attack_table", 0xDADC, 19),
    # The channel allocator's scan order: 16 bytes, one channel index each,
    # at runtime 0x03805964 (hal/sdat/mixer.cpp port_arm7_scan_order). Stays
    # after the attack table: arm7_image() checks row 0's shape.
    ("port_arm7_scan_order", 0xDACC, 16),
]

# OVERLAY SNAPSHOTS. A port-named copy of an overlay span the port compares a
# live mount against, so the comparison keeps a pristine ROM side even after
# the mount's own words are replaced. Offsets are into the decompressed
# overlay file (extracted/overlays/overlay_NNNN.bin).
#   (name, overlay id, file offset, size, the typeinfo its vtable header
#    names, header bytes ahead of the row)
OV_ROWS = [
    # dEntObj_c's 18 vtable slots (_ZTV9dEntObj_c + 8, past offset-to-top and
    # typeinfo): hal/scene_vs_menu.cpp checks each live slot against these
    # words before it seats the host body (it carried them as kEntRom until
    # lane ROMDATA).
    ("port_ent_obj_vtable_rom", 75, 0x87C0, 72, '_ZTI9dEntObj_c', 8),
]

# NAMED ROWS A LINKED DECOMP TU NOW DEFINES (catchup1, SYM4's Stage C two-storage probe). At the bump
# KuppaScript.cpp and Sound.cpp define these objects themselves, at the same ROM addresses, as C++
# globals; a second copy here would be two storages for one address, and the port's writes and the
# TU's reads would part. The TU's copy wins: the row is not emitted, and romdata.c aliases the C
# spelling port code may still use onto the TU's decorated name (/alternatename binds only when
# nothing else defines the C name).
#   name: (decorated name in the TU's object, ROM address, TU)
TU_DEFINED = {
    "sCutsceneIntroEnd": ("?sCutsceneIntroEnd@@3PAEA", 0x020876d4, "KuppaScript.cpp"),
    "sCutsceneEndingStartMusic": ("?sCutsceneEndingStartMusic@@3PAEA", 0x020876fc, "KuppaScript.cpp"),
    "sCutsceneEndingMarioLandHold": ("?sCutsceneEndingMarioLandHold@@3PAEA", 0x0208771c, "KuppaScript.cpp"),
    "sCutsceneGrandStarFly": ("?sCutsceneGrandStarFly@@3PAEA", 0x02087740, "KuppaScript.cpp"),
    "sEndingMarioToPeachFocusPoints": ("?sEndingMarioToPeachFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x02087798, "KuppaScript.cpp"),
    "sTHIWigglerCreditsSplineFocusPoints": ("?sTHIWigglerCreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x020877c8, "KuppaScript.cpp"),
    "sEndingMarioToPeachPosPoints": ("?sEndingMarioToPeachPosPoints@@3PAUCutsceneSplinePoint@@A", 0x020877f8, "KuppaScript.cpp"),
    "sTHIWigglerCreditsSplinePositionsPoints": ("?sTHIWigglerCreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x02087828, "KuppaScript.cpp"),
    "sCutsceneCreditsCCMOutside": ("?sCutsceneCreditsCCMOutside@@3PAEA", 0x02087858, "KuppaScript.cpp"),
    "sCutsceneCreditsDDDSub": ("?sCutsceneCreditsDDDSub@@3PAEA", 0x0208788c, "KuppaScript.cpp"),
    "sCutsceneCreditsCotMC": ("?sCutsceneCreditsCotMC@@3PAEA", 0x020878c0, "KuppaScript.cpp"),
    "sCutsceneCreditsSA": ("?sCutsceneCreditsSA@@3PAEA", 0x020878f4, "KuppaScript.cpp"),
    "sCutsceneCreditsRR": ("?sCutsceneCreditsRR@@3PAEA", 0x02087928, "KuppaScript.cpp"),
    "sCutsceneCreditsTTC": ("?sCutsceneCreditsTTC@@3PAEA", 0x0208795c, "KuppaScript.cpp"),
    "sCutsceneCreditsTHIHuge": ("?sCutsceneCreditsTHIHuge@@3PAEA", 0x02087990, "KuppaScript.cpp"),
    "sCutsceneCreditsTTM": ("?sCutsceneCreditsTTM@@3PAEA", 0x020879c4, "KuppaScript.cpp"),
    "sCutsceneCreditsWDW": ("?sCutsceneCreditsWDW@@3PAEA", 0x020879f8, "KuppaScript.cpp"),
    "sCutsceneCreditsSL": ("?sCutsceneCreditsSL@@3PAEA", 0x02087a2c, "KuppaScript.cpp"),
    "sCutsceneCreditsDDD": ("?sCutsceneCreditsDDD@@3PAEA", 0x02087a60, "KuppaScript.cpp"),
    "sCutsceneCreditsSSL": ("?sCutsceneCreditsSSL@@3PAEA", 0x02087a94, "KuppaScript.cpp"),
    "sCutsceneCreditsVolcano": ("?sCutsceneCreditsVolcano@@3PAEA", 0x02087ac8, "KuppaScript.cpp"),
    "sCutsceneCreditsTHIWiggler": ("?sCutsceneCreditsTHIWiggler@@3PAEA", 0x02087afc, "KuppaScript.cpp"),
    "sCutsceneCreditsHMC": ("?sCutsceneCreditsHMC@@3PAEA", 0x02087b30, "KuppaScript.cpp"),
    "sCutsceneCreditsBBH": ("?sCutsceneCreditsBBH@@3PAEA", 0x02087b64, "KuppaScript.cpp"),
    "sCutsceneCreditsJRB": ("?sCutsceneCreditsJRB@@3PAEA", 0x02087b98, "KuppaScript.cpp"),
    "sCutsceneCreditsWF": ("?sCutsceneCreditsWF@@3PAEA", 0x02087bcc, "KuppaScript.cpp"),
    "sCutsceneCreditsBoB": ("?sCutsceneCreditsBoB@@3PAEA", 0x02087c00, "KuppaScript.cpp"),
    "sCutsceneCreditsCCMSlide": ("?sCutsceneCreditsCCMSlide@@3PAEA", 0x02087c34, "KuppaScript.cpp"),
    "sCCMOutsideCreditsSplineFocusPoints": ("?sCCMOutsideCreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x02087c68, "KuppaScript.cpp"),
    "sBBHCreditsSplinePositionsPoints": ("?sBBHCreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x02087ca0, "KuppaScript.cpp"),
    "sCCMOutsideCreditsSplinePositionsPoints": ("?sCCMOutsideCreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x02087cd8, "KuppaScript.cpp"),
    "sSLCreditsSplineFocusPoints": ("?sSLCreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x02087d10, "KuppaScript.cpp"),
    "sSLCreditsSplinePositionsPoints": ("?sSLCreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x02087d48, "KuppaScript.cpp"),
    "sWDWCreditsSplineFocusPoints": ("?sWDWCreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x02087d80, "KuppaScript.cpp"),
    "sWDWCreditsSplinePositionsPoints": ("?sWDWCreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x02087db8, "KuppaScript.cpp"),
    "sTTCCreditsSplineFocusPoints": ("?sTTCCreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x02087df0, "KuppaScript.cpp"),
    "sTTCCreditsSplinePositionsPoints": ("?sTTCCreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x02087e28, "KuppaScript.cpp"),
    "sRRCreditsSplineFocusPoints": ("?sRRCreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x02087e60, "KuppaScript.cpp"),
    "sRRCreditsSplinePositionsPoints": ("?sRRCreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x02087e98, "KuppaScript.cpp"),
    "sEndingLookAtSkyFocusPoints": ("?sEndingLookAtSkyFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x02087ed0, "KuppaScript.cpp"),
    "sBBHCreditsSplineFocusPoints": ("?sBBHCreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x02087f08, "KuppaScript.cpp"),
    "sCutsceneEndingMarioLandCloseup": ("?sCutsceneEndingMarioLandCloseup@@3PAEA", 0x02087f40, "KuppaScript.cpp"),
    "sDDDSubCreditsSplineFocusPoints": ("?sDDDSubCreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x02087f80, "KuppaScript.cpp"),
    "sJRBCreditsSplinePositionsPoints": ("?sJRBCreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x02087fc0, "KuppaScript.cpp"),
    "sCCMSlideCreditsSplineFocusPoints": ("?sCCMSlideCreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x02088000, "KuppaScript.cpp"),
    "sDDDSubCreditsSplinePositionsPoints": ("?sDDDSubCreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x02088040, "KuppaScript.cpp"),
    "sCCMSlideCreditsSplinePositionsPoints": ("?sCCMSlideCreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x02088080, "KuppaScript.cpp"),
    "sJRBCreditsSplineFocusPoints": ("?sJRBCreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x020880c0, "KuppaScript.cpp"),
    "sBoBCreditsSplineFocusPoints": ("?sBoBCreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x02088100, "KuppaScript.cpp"),
    "sSACreditsSplineFocusPoints": ("?sSACreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x02088140, "KuppaScript.cpp"),
    "sSACreditsSplinePositionsPoints": ("?sSACreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x02088180, "KuppaScript.cpp"),
    "sCotMCCreditsSplineFocusPoints": ("?sCotMCCreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x020881c0, "KuppaScript.cpp"),
    "sBoBCreditsSplinePositionsPoints": ("?sBoBCreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x02088200, "KuppaScript.cpp"),
    "sEndingLookUpAtCastlePoints": ("?sEndingLookUpAtCastlePoints@@3PAUCutsceneSplinePoint@@A", 0x02088240, "KuppaScript.cpp"),
    "sWFCreditsSplineFocusPoints": ("?sWFCreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x02088280, "KuppaScript.cpp"),
    "sCotMCCreditsSplinePositionsPoints": ("?sCotMCCreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x020882c0, "KuppaScript.cpp"),
    "sWFCreditsSplinePositionsPoints": ("?sWFCreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x02088300, "KuppaScript.cpp"),
    "sCutsceneEndingMarioLand": ("?sCutsceneEndingMarioLand@@3PAEA", 0x02088340, "KuppaScript.cpp"),
    "sVolcanoCreditsSplineFocusPoints": ("?sVolcanoCreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x020883d0, "KuppaScript.cpp"),
    "sVolcanoCreditsSplinePositionsPoints": ("?sVolcanoCreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x02088418, "KuppaScript.cpp"),
    "sSSLCreditsSplineFocusPoints": ("?sSSLCreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x02088460, "KuppaScript.cpp"),
    "sSSLCreditsSplinePositionsPoints": ("?sSSLCreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x020884a8, "KuppaScript.cpp"),
    "sTTMCreditsSplineFocusPoints": ("?sTTMCreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x020884f0, "KuppaScript.cpp"),
    "sTTMCreditsSplinePositionsPoints": ("?sTTMCreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x02088538, "KuppaScript.cpp"),
    "gEndWavingFocusPoints": ("?gEndWavingFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x02088580, "KuppaScript.cpp"),
    "gEndWavingPosPoints": ("?gEndWavingPosPoints@@3PAUCutsceneSplinePoint@@A", 0x020885c8, "KuppaScript.cpp"),
    "sCutsceneGrandStar": ("?sCutsceneGrandStar@@3PAEA", 0x02088610, "KuppaScript.cpp"),
    "sHMCCreditsSplineFocusPoints": ("?sHMCCreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x02088660, "KuppaScript.cpp"),
    "sHMCCreditsSplinePositionsPoints": ("?sHMCCreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x020886b0, "KuppaScript.cpp"),
    "sDDDCreditsSplineFocusPoints": ("?sDDDCreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x02088700, "KuppaScript.cpp"),
    "sDDDCreditsSplinePositionsPoints": ("?sDDDCreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x02088750, "KuppaScript.cpp"),
    "sTHIHugeCreditsSplineFocusPoints": ("?sTHIHugeCreditsSplineFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x020887a0, "KuppaScript.cpp"),
    "sTHIHugeCreditsSplinePositionsPoints": ("?sTHIHugeCreditsSplinePositionsPoints@@3PAUCutsceneSplinePoint@@A", 0x020887f0, "KuppaScript.cpp"),
    "sEndingFlyToWindowFocusPoints": ("?sEndingFlyToWindowFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x02088840, "KuppaScript.cpp"),
    "sEndingFlyToWindowPosPoints": ("?sEndingFlyToWindowPosPoints@@3PAUCutsceneSplinePoint@@A", 0x02088898, "KuppaScript.cpp"),
    "sCutsceneEndingStarsFreePeach": ("?sCutsceneEndingStarsFreePeach@@3PAEA", 0x0208894c, "KuppaScript.cpp"),
    "sCutsceneDanceRotate": ("?sCutsceneDanceRotate@@3PAEA", 0x020889b0, "KuppaScript.cpp"),
    "sCutsceneEndingDialog": ("?sCutsceneEndingDialog@@3PAEA", 0x02088a18, "KuppaScript.cpp"),
    "sCutsceneIntroPeachFlyToPipe": ("?sCutsceneIntroPeachFlyToPipe@@3PAEA", 0x02088a94, "KuppaScript.cpp"),
    "sCutsceneEndingMarioToPeach": ("?sCutsceneEndingMarioToPeach@@3PAEA", 0x02088ba0, "KuppaScript.cpp"),
    "sEndingPeachDescentCamPosPoints": ("?sEndingPeachDescentCamPosPoints@@3PAUCutsceneSplinePoint@@A", 0x02088c2c, "KuppaScript.cpp"),
    "sCutsceneEndingPeachDescends": ("?sCutsceneEndingPeachDescends@@3PAEA", 0x02088ccc, "KuppaScript.cpp"),
    "sIntroStartToPipeFocusPoints": ("?sIntroStartToPipeFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x02088d80, "KuppaScript.cpp"),
    "sIntroStartToPipePositionPoints": ("?sIntroStartToPipePositionPoints@@3PAUCutsceneSplinePoint@@A", 0x02088e38, "KuppaScript.cpp"),
    "sCutsceneEndingKiss": ("?sCutsceneEndingKiss@@3PAEA", 0x02088ef0, "KuppaScript.cpp"),
    "sCutsceneEndWaving": ("?sCutsceneEndWaving@@3PAEA", 0x02088fb8, "KuppaScript.cpp"),
    "sCutsceneIntroPeachLetter": ("?sCutsceneIntroPeachLetter@@3PAEA", 0x020890a0, "KuppaScript.cpp"),
    "sIntroPipeToRoofFocusPoints": ("?sIntroPipeToRoofFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x020891a8, "KuppaScript.cpp"),
    "sIntroPipeToRoofPositionPoints": ("?sIntroPipeToRoofPositionPoints@@3PAUCutsceneSplinePoint@@A", 0x020892c0, "KuppaScript.cpp"),
    "gIntroLakituStartToPipeOffsetFromCameraPoints": ("?gIntroLakituStartToPipeOffsetFromCameraPoints@@3PAUCutsceneSplinePoint@@A", 0x020893d8, "KuppaScript.cpp"),
    "gIntroLakituStartToPipeFocusPoints": ("?gIntroLakituStartToPipeFocusPoints@@3PAUCutsceneSplinePoint@@A", 0x020894f0, "KuppaScript.cpp"),
    "sCutsceneEndingMarioFall": ("?sCutsceneEndingMarioFall@@3PAEA", 0x02089608, "KuppaScript.cpp"),
    "sCutsceneEndingPeachWakeup": ("?sCutsceneEndingPeachWakeup@@3PAEA", 0x02089850, "KuppaScript.cpp"),
    "sCutsceneIntroPeachMarioAppears": ("?sCutsceneIntroPeachMarioAppears@@3PAEA", 0x02089af8, "KuppaScript.cpp"),
    "gSubSoundID": ("?gSubSoundID@@3HA", 0x0208e430, "Sound.cpp"),
    "gMusicTrackModeOverride": ("?gMusicTrackModeOverride@@3HA", 0x0208e434, "Sound.cpp"),
}


# Address RUNS whose symbols must stay adjacent in host memory because the
# ROM code walks ACROSS the symbol boundaries. _ZN9dCamera_c7SetModeEi sets the camera's
# mode pointer to `index * 0x28 + &data_02086fcc`, and Camera::Behavior /
# Camera::Render then compare that pointer against &data_0208733c and
# &data_0208738c -- symbols 15 and 18 records further along. Emitted as one
# array per symbol is byte-correct but places them at unrelated addresses, so
# every mode past the first reads garbage and no comparison can ever hold.
#
# MSVC grouped sections put them back together: the linker concatenates
# contributions to `.name$suffix` sorted by the full section name, so one
# $NNN per symbol in address order lays the run out in ROM order. Each run
# carries the alignment its deltas share (a delta that is not a multiple of the
# alignment would pad and break the run), and the HAL asserts the result at
# runtime.
#
# (tag, start, end, align). align defaults to 8.
CONTIG = [
    # the camera-mode preset table: 33 records of 0x28, 0x528 total
    ("cammod", 0x02086FCC, 0x020874F4, 8),
    # THE OBJECT-MESSAGE REMAP TABLE, read as TWO OVERLAPPING VIEWS.
    # ObjectMessageIDToActualMessageID walks one interleaved {key,value} u16
    # table at 0x0208eeec: data_0208eeec[i].v (stride 4) is the key column and
    # data_0208eeee[i].v (= data_0208eeec+2, same stride) is the value column.
    # dsd named both halves, so the delta-to-next-symbol rule sizes data_0208eeec
    # at 2 bytes -- and the walk then indexes a 4-byte host array off its end,
    # remapping the sign/NPC object id to garbage so St_Talk_Main's box opens on
    # an out-of-range text id and never activates. Laid out contiguously the two
    # views alias the same bytes exactly as on the DS. The run runs to
    # data_0208f021 (dsd's next name inside the same table, at an ODD address --
    # dsd split the run mid-halfword); the walk reads keys up to index 0x61, past
    # that name, so it has to be one host array too. align 1: the members pack
    # byte-tight so every symbol lands at its exact ROM-relative offset, which is
    # the whole point -- data_0208eeee must sit at data_0208eeec+2. x86 tolerates
    # the unaligned u16 reads the Pair view then does.
    ("objmsg", 0x0208EEEC, 0x0208F074, 1),
    # GX_BeginLoadTex's texture-bank start table: 16 GXTexStartAddr rows of
    # {blk1, blk2, szBlk1} u16, indexed by the bank combination. dsd split it
    # into data_02086324 (2 bytes), data_02086326 (2) and data_02086328 (0x5c),
    # and hosted apart each was padded to 8, so row 3 (banks A+B, the game's
    # own combination) read the wrong bytes: blk1 0, and GX_LoadTex copied to
    # the raw slot offset (0x0001e000..0x0001ff00). One run keeps every row at
    # its ROM offset.
    ("gxtex", 0x02086324, 0x02086384, 2),
    # The overlay digest table and its end marker (lane ROMDATA, from
    # hal/boot_globals.cpp). src/engine/fs/FS_Overlay.c counts the overlays as
    # (SDK_OVERLAY_DIGEST_END - SDK_OVERLAY_DIGEST) / 20, so the two symbols
    # must stay adjacent: 2060 + 8 bytes, reloc-free, ending at BSS_START.
    ("ovdigest", 0x0209A7EC, 0x0209B000, 4),
]


def symbol_table(root):
    import re
    syms = []
    for line in (root / "config/arm9/symbols.txt").read_text().splitlines():
        m = re.search(r"^(\S+)\s+kind:\S+\s+addr:0x([0-9a-fA-F]+)", line)
        if m:
            syms.append((int(m.group(2), 16), m.group(1)))
    syms.sort()
    return syms


def contig_entries(syms):
    """[(tag, align, [(name, addr, size), ...])] for every run in CONTIG."""
    runs = []
    for tag, start, end, align in CONTIG:
        members = [(a, n) for a, n in syms if start <= a < end]
        out = []
        for i, (a, n) in enumerate(members):
            nxt = members[i + 1][0] if i + 1 < len(members) else end
            size = nxt - a
            if size % align:
                sys.exit(f"{tag}: {n} is {size:#x} bytes, not a multiple of "
                         f"{align} -- ordered sections would pad and break the "
                         "run")
            out.append((n, a, size))
        if not out or out[0][1] != start:
            sys.exit(f"{tag}: no symbol at the run start {start:#x}")
        runs.append((tag, align, out))
    return runs


def named_entries(root, syms):
    addr_of = {n: a for a, n in syms}
    contig_names = {n for _, _, mem in contig_entries(syms) for n, _, _ in mem}
    out = []
    for entry in NAMED:
        # "name:0xSIZE" pins the extent explicitly. The default -- delta to the
        # next symbol, the ovdata.py convention -- is only correct when the
        # config happens to have no symbol inside the object, and for anything
        # the game reads as one blob that is an assumption, not a fact: the
        # config carries page-aligned boundary symbols (0x02078000, 0x02080000)
        # that are not real objects and silently cut whatever spans them. The
        # other reason for a pin: a neighbour that is a COLUMN of the same
        # table rather than the next table (see data_02075768).
        name, _, override = entry.partition(":")
        if name in contig_names:
            continue   # the run owns it
        if name in TU_DEFINED:
            continue   # a linked decomp TU owns it (see TU_DEFINED)
        a = addr_of[name]
        if override:
            out.append((name, a, int(override, 0)))
            continue
        nxt = next((s for s, _ in syms if s > a), a + 4)
        out.append((name, a, max(4, nxt - a)))
    return out


def main():
    root = decomp_root.path(decomp_root.find())
    # LINK-ONLY (see port/tools/link_only.py). With no cartridge, stand a
    # config-length image in for arm9_dec.bin and skip the three checks below,
    # every one of which asks "is this image the real one" -- a question with
    # no meaningful answer here, and one this mode answers NO to in the loudest
    # way it can (the archive paths below become the marker string, so the exe
    # cannot open a single file). Nothing about WHICH symbols are emitted or
    # how they are sized changes: those come from TABLES, symbols.txt and
    # BSS_START either way.
    link_only.set_link_only(sys.argv)
    if link_only.LINK_ONLY:
        data = link_only.synth_image(root, "arm9", base=BASE,
                                     length=BSS_START - BASE)
        link_only.stamp(root)
        return _emit(root, data, arm7_link_only(), ov_link_only())

    img = root / "extracted/arm9_dec.bin"
    if not img.exists():
        sys.exit(f"missing {img} -- extract the ROM first")
    anchor = img.read_bytes()
    a = 0x0205A548 - BASE
    if anchor[a:a + 4] != b"\x0c\x10\xb0\xe8":
        sys.exit("arm9_dec.bin failed the MI_Copy36B anchor -- wrong image/base")

    # The particle archive is the one NAMED entry whose true length is written
    # down inside it, so check the pin against the header rather than trusting
    # the comment next to it. Worth checking because getting this wrong is
    # SILENT: the parse still succeeds and the effects that fell off the end
    # simply never emit.
    spa = 0x02075F14 - BASE
    if anchor[spa:spa + 8] != b" APS10_1":
        sys.exit("no SPA magic at 0x02075f14 -- the particle blob moved")
    tex_size, tex_off = struct.unpack_from("<II", anchor, spa + 0x14)
    spa_size = tex_off + tex_size
    pinned = next((int(e.partition(":")[2], 0) for e in NAMED
                   if e.startswith("data_02075f14:")), None)
    if pinned != spa_size:
        sys.exit(f"particle blob is pinned at {pinned:#x} but its own SPA "
                 f"header says {spa_size:#x} -- fix the pin in NAMED")
    for addr, length, _ in TABLES:
        if addr + length > BSS_START:
            sys.exit(f"{addr:#x}+{length:#x} crosses into BSS -- not file-backed")
    data = img.read_bytes()
    return _emit(root, data, arm7_image(root), ov_images(root))


def arm7_link_only():
    """LINK-ONLY's stand-in for extracted/arm7.bin: zeros, long enough for
    every ARM7_ROWS slice (nothing about the link depends on the values)."""
    return bytes(max(off + size for _, off, size in ARM7_ROWS))


def ov_link_only():
    """LINK-ONLY's stand-in for the OV_ROWS overlays: zeros, long enough."""
    out = {}
    for _n, ovid, off, size, _s, _d in OV_ROWS:
        out[ovid] = bytes(max(len(out.get(ovid, b"")), off + size))
    return out


def ov_images(root):
    """The OV_ROWS overlay images, each row checked before it is cut: the two
    words ahead of it must be the vtable header {0, &typeinfo} with the
    typeinfo address config names, and every slot must be a DS address, so a
    wrong offset or a wrong file cannot hand over other bytes. The offset is
    into the ROM's own overlay file; the dsd ov copies are laid out
    differently and are only a fallback."""
    import re as _re
    out = {}
    for name, ovid, off, size, tinfo, _hdr in OV_ROWS:
        if ovid not in out:
            p = root / f"extracted/overlays/overlay_{ovid:04d}.bin"
            if not p.exists():
                p = root / f"extracted/dsd/arm9_overlays/ov{ovid:03d}.bin"
            if not p.exists():
                sys.exit(f"missing {p} -- extract the ROM first")
            out[ovid] = p.read_bytes()
        img = out[ovid]
        addr = None
        for line in (root / f"config/arm9/overlays/ov{ovid:03d}/symbols.txt").read_text().splitlines():
            if line.split()[:1] == [tinfo]:
                addr = int(_re.search(r"addr:0x([0-9a-f]+)", line).group(1), 16)
        if off < 8 or off + size > len(img):
            sys.exit(f"romdata: {name} falls outside overlay {ovid}")
        hdr = (int.from_bytes(img[off - 8:off - 4], "little"),
               int.from_bytes(img[off - 4:off], "little"))
        if addr is None or hdr != (0, addr):
            sys.exit(f"romdata: {name} at {off:#x} does not follow a "
                     f"{{0, {tinfo}}} vtable header in ov{ovid:03d}")
        for i in range(off, off + size, 4):
            w = int.from_bytes(img[i:i + 4], "little")
            if not 0x02000000 <= w < 0x02400000:
                sys.exit(f"romdata: {name} word +{i - off:#x} = {w:#x} is not a DS address")
    return out


def arm7_image(root):
    """extracted/arm7.bin, checked before any row is cut from it.

    The rows are file offsets, so a wrong file would hand over wrong bytes in
    silence. The one table read today has a shape that says whether the offset
    landed on it: it starts at 0 and rises strictly (an attack rate per attack
    value), which a misplaced window of code or data does not do."""
    img = root / "extracted/arm7.bin"
    if not img.exists():
        sys.exit(f"missing {img} -- extract the ROM first")
    data = img.read_bytes()
    for name, off, size in ARM7_ROWS:
        if off + size > len(data):
            sys.exit(f"romdata: {name} at arm7.bin+{off:#x} runs past the "
                     f"end of the image ({len(data):#x} bytes)")
    name, off, size = ARM7_ROWS[0]
    t = data[off:off + size]
    if t[0] != 0 or any(b <= a for a, b in zip(t, t[1:])):
        sys.exit(f"romdata: {name} at arm7.bin+{off:#x} is not the attack "
                 "table (expected 0 then strictly rising) -- wrong arm7.bin?")
    return data


def _emit(root, data, arm7, ovs):
    """Emit romdata.c from one arm9 image, real or synthesised.

    Split out of main() so LINK-ONLY reaches the IDENTICAL emitter: which
    symbols exist, how they are sized, which sections they are grouped into
    and what the file declares are all decided here, from TABLES,
    config/arm9/symbols.txt and BSS_START, and none of them looks at a value.
    """
    # ROM-CLEAN (2026-08-10, --rom-clean): the shipped exe must embed ZERO ROM
    # bytes. Under the flag every array below is emitted ZEROED and a boot-time
    # loader memcpys the real bytes out of build/assets/romdata.bin. Without the
    # flag (dev default) the arrays keep their bytes exactly as before.
    #
    #   collect(name, raw_bytes): records one blob entry AND returns the C brace
    #   body -- "0" (zeroed, bytes go to the sidecar part) under --rom-clean, the
    #   real bytes otherwise. Byte-honesty: the slice for each symbol is the
    #   identical bytearray the baked emitter prints, so post-load bytes are
    #   byte-identical to the baked build -- only WHERE they live changed. The
    #   loader (port/hal/romdata_loader.cpp) verifies each slice size and a
    #   whole-file sha against the manifest and aborts loudly on any mismatch.
    romblob_common.set_rom_clean(sys.argv)
    entries = []            # (symbol_name, raw_bytes) in emission (blob) order

    def collect(name, raw):
        entries.append((name, bytes(raw)))
        return "{ %s }" % romblob_common.init_body(raw)

    out_path = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else (
        pathlib.Path(decomp_root.PORT_TREE) / "build/port/host-src/romdata.c")
    out_path.parent.mkdir(parents=True, exist_ok=True)

    lines = ["/* GENERATED by port/tools/romdata.py from extracted/arm9_dec.bin.",
             " * ROM constants stay out of git; regenerate locally. */"]
    if link_only.LINK_ONLY:
        lines.append(link_only.banner("arm9_dec.bin"))
    if romblob_common.ROM_CLEAN:
        # --rom-clean zeroes the arrays; the bytes load from romdata.bin.
        lines.append("#include <string.h>")   # for the apply memcpy
    lines.append("")
    # THE ARM9 .data THIS FILE HOSTS IS GAME STATE, NOT A TABLE OF CONSTANTS. The
    # DS writes it (_Z12SetNextLeveli's return-to-level latch gReturnSublevel, the
    # loaded-song word sLayer1Music, the minigame menu's selection
    # gMgLastSelected ...), so a state capture that leaves it out is half a restore:
    # a restore put the world back and left the return latch at the -1 the first star
    # exit had written, and the next star exit came out at the castle front
    # instead of the painting. These arrays used to sit in the plain .data that
    # the snapshot never copies, on the claim that this file only emits
    # read-only ROM constants; that claim was false for every symbol the port
    # had to host WITH a ROM initial value. Route all of it into the section the
    # snapshot captures wholesale (hal/dsstate_seg.h), so no list has to be
    # kept of which of these the game writes. About 120 KB. The section
    # declaration comes first for the reason ovdata.py gives (LNK4078).
    lines.append('#pragma section(".dsstate$mmm", read, write)')
    lines.append('#pragma data_seg(".dsstate$mmm")')
    lines.append('#pragma bss_seg(".dsstate$mmm")')
    for addr, length, ctype in TABLES:
        off = addr - BASE
        blob = data[off:off + length]
        if len(blob) != length:
            sys.exit(f"short read at {addr:#x}")
        # element width + unpack format PER TYPE. A missing entry is a hard
        # error rather than a silent fallback: the old `4 if int else 2` read
        # the byte tables as halfwords and emitted half as many, half-wrong
        # elements (see TABLES).
        ELEM = {
            "int": (4, "<i"), "unsigned int": (4, "<I"),
            "short": (2, "<h"), "unsigned short": (2, "<H"),
            "char": (1, "<b"), "unsigned char": (1, "<B"),
        }
        if ctype not in ELEM:
            sys.exit(f"romdata: no element width for C type {ctype!r}")
        width, fmt = ELEM[ctype]
        if length % width:
            sys.exit(f"romdata: {addr:#x} length {length} is not a multiple "
                     f"of {width} ({ctype})")
        # collect() records the raw bytes for the blob (the loader memcpys them,
        # which on host x86-LE == the DS bytes). The C body must be TYPED element
        # values, not raw bytes -- an int[12] cannot take 48 byte initialisers.
        # Under --rom-clean collect() zeroes the body; otherwise emit the real
        # element values here.
        name = f"data_{addr:08x}"
        collect(name, blob)
        if romblob_common.ROM_CLEAN:
            body = "0"
        else:
            vals = [struct.unpack_from(fmt, blob, i)[0]
                    for i in range(0, length, width)]
            body = ", ".join(str(v) for v in vals)
        lines.append(f"{ctype} {name}[{length // width}] = {{ {body} }};")
        lines.append("")
    syms = symbol_table(root)
    named_emitted = set()
    for name, addr, size in named_entries(root, syms):
        if addr + size > BSS_START:
            size = BSS_START - addr
        blob = data[addr - BASE:addr - BASE + size]
        named_emitted.add(name)
        lines.append(f"__declspec(align(8)) unsigned char {name}[{size}] = "
                     f"{collect(name, blob)};")
    lines.append("")
    # Port-named arm9 rows and ARM7 program constants (see HOST_ROWS and
    # ARM7_ROWS): the same emission as a NAMED row, from their own sources.
    for name, addr, size in HOST_ROWS:
        if addr + size > BSS_START:
            sys.exit(f"romdata: {name} {addr:#x}+{size:#x} crosses into BSS")
        blob = data[addr - BASE:addr - BASE + size]
        lines.append(f"__declspec(align(8)) unsigned char {name}[{size}] = "
                     f"{collect(name, blob)};")
    for name, off, size in ARM7_ROWS:
        blob = arm7[off:off + size]
        lines.append(f"__declspec(align(8)) unsigned char {name}[{size}] = "
                     f"{collect(name, blob)};")
    for name, ovid, off, size, _sym, _delta in OV_ROWS:
        blob = ovs[ovid][off:off + size]
        lines.append(f"__declspec(align(8)) unsigned char {name}[{size}] = "
                     f"{collect(name, blob)};")
    lines.append("")
    lines.append("/* NAMED rows a linked decomp TU defines (TU_DEFINED): the TU's copy is the storage. */")
    for name, (decorated, _addr, _tu) in sorted(TU_DEFINED.items()):
        lines.append(f'#pragma comment(linker, "/alternatename:_{name}={decorated}")')
    lines.append("")

    # Grouped-section runs (see CONTIG).
    for tag, align, members in contig_entries(syms):
        lines.append(f"/* run .{tag}: {members[0][1]:#010x} .. "
                     f"{members[-1][1] + members[-1][2]:#010x}, "
                     f"{len(members)} symbols, laid out in ROM order "
                     f"(align {align}) */")
        for i, (name, addr, size) in enumerate(members):
            # the run stays contiguous inside the captured section: sections
            # of one family sort by the text after the `$`, and no other
            # contribution to .dsstate carries this prefix
            sec = f".dsstate$rom{tag}{i:04d}"
            lines.append(f'#pragma section("{sec}", read, write)')
            blob = data[addr - BASE:addr - BASE + size]
            lines.append(f'__declspec(allocate("{sec}")) '
                         f'__declspec(align({align})) '
                         f"unsigned char {name}[{size}] = {collect(name, blob)};")
        first, last = members[0], members[-1]
        lines.append(f"unsigned {tag}_run_base = (unsigned)&{first[0]}[0];")
        lines.append(f"unsigned {tag}_run_end = (unsigned)&{last[0]}[0] + "
                     f"{last[2]};")
        lines.append(f"unsigned {tag}_run_span = "
                     f"{members[-1][1] + members[-1][2] - members[0][1]};")
    lines.append("")

    # Everything above is hosted DS storage and is inside the captured section;
    # what follows is host-shaped (string-literal pointers) and stays out.
    lines.append('#pragma data_seg()')
    lines.append('#pragma bss_seg()')
    lines.append("")

    # The archive-mount table at data_0208ecf4: 13 entries of
    # {ptr, heap, u16 idBase, u16 idEnd, char *shortName, char *narcPath}.
    # Interior file IDs (>= 0x8000) resolve as narc[id - idBase]; the fs
    # seam consumes this host-shaped copy (see port/hal/fs.cpp).
    #
    # NOT a ROM-content array: it holds host `const char *` string literals (the
    # narc paths) and host-shaped {base,end} ids, not raw ROM bytes, so it stays
    # a real initialiser -- see the syms-pack verdict in out/rom-clean/DESIGN.md.
    lines.append("struct port_arc_entry { unsigned short base, end;"
                 " const char *narc; };")
    #
    # LINK-ONLY: the ids and the path string are the one place in this file
    # where an emitted VALUE is read out of the image and then followed (the
    # path pointer is chased into the image for the string). A synthesised
    # image has a zero there, so rather than emit thirteen empty strings this
    # emits the marker -- the exe then cannot mount an archive, fails at the fs
    # seam on its first file open, and carries a greppable string saying why.
    # The table's SHAPE (13 entries of {u16, u16, const char *}) is a constant
    # here and does not move, so nothing about the link changes.
    ents = []
    name_ptrs = []
    path_ptrs = []
    off = 0x0208ECF4 - BASE
    for i in range(13):
        if link_only.LINK_ONLY:
            ents.append('    { 0, 0, "%s" },' % link_only.MARKER)
            name_ptrs.append("0")
            path_ptrs.append("0")
            continue
        blob = data[off + i*0x14: off + (i+1)*0x14]
        base_id, end_id = struct.unpack_from("<HH", blob, 8)
        name_ptr = struct.unpack_from("<I", blob, 0x0c)[0]
        path_ptr = struct.unpack_from("<I", blob, 0x10)[0]
        p = data[path_ptr - BASE:path_ptr - BASE + 64].split(b"\0")[0]
        ents.append('    { %d, %d, "%s" },' % (base_id, end_id,
                                               p.decode("ascii").lstrip("/")))
        # The entry's OWN two words name the symbols that hold those strings, so
        # nothing here is typed and nothing is kept in a hand-written order --
        # the two orders genuinely disagree (entry 0's short name is the second
        # symbol in the name run, and the paths are not in entry order either).
        # A pointer with no NAMED row is a hard error rather than a quiet null:
        # a null is exactly what the ROM's mount branch would then open.
        for ptr, dest in ((name_ptr, name_ptrs), (path_ptr, path_ptrs)):
            sym = "data_%08x" % ptr
            if sym not in named_emitted:
                sys.exit("romdata: mount entry %d points at %#010x and %s is "
                         "not in NAMED -- add it" % (i, ptr, sym))
            dest.append("(const char *)%s" % sym)
    lines.append("struct port_arc_entry port_archive_map[13] = {")
    lines.extend(ents)
    lines.append("};")
    lines.append("")

    # The two DS string pointers each mount entry carries, resolved to the
    # hosted ROM bytes above. hal/card_mount.cpp writes these into
    # data_0208ecf4's +0x0c and +0x10 as it publishes, so the ROM's own mount
    # branch reads a real three-character archive name and a real NitroFS path
    # where lane CARDFS had to leave a null.
    for arr, vals in (("port_archive_name", name_ptrs),
                      ("port_archive_path", path_ptrs)):
        lines.append("const char *%s[13] = {" % arr)
        lines.extend("    %s," % v for v in vals)
        lines.append("};")
    lines.append("")

    # The boot-time loader face (--rom-clean only). romblob.py's
    # port_romdata_load_all() calls this with a pointer to romdata's section of
    # the verified romdata.bin; each memcpy fills one now-zeroed array. Emitting
    # it here (not centrally) keeps it in the one TU that names every symbol. In
    # a baked build emit_apply returns nothing, so the file is byte-identical to
    # the pre-ROM-CLEAN output.
    apply = romblob_common.emit_apply("romdata", entries)
    if apply:
        lines.append("/* Fill the zeroed arrays above from romdata.bin (already"
                     " read + verified). */")
        lines.extend(apply)
        lines.append("")
    out_path.write_text("\n".join(lines), encoding="ascii")

    # The sidecar blob PART. romblob.py concatenates every emitter's part into
    # build/assets/romdata.bin and writes the whole-file manifest + dispatch.
    total = romblob_common.write_part(out_path, "romdata", entries)
    print(f"romdata -> {out_path} ({total} ROM bytes moved to {out_path}.blobpart)")


if __name__ == "__main__":
    main()
