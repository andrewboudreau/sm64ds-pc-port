// THE MINIGAME SOUND RECORD, and the ov005 row that seeds it. Run link60,
// lane MUS1. The owner reported "no music" on the curling minigame
// (SM64DS_SCENE=374); this file is the fix, and it transcribes ZERO ROM
// function bodies -- it re-hosts one arm9 BSS record at its true size and
// copies one verbatim ROM DATA row into it, which is what the scene's own
// (already linked) framework then reads.
//
// ---- 1. WHY THERE WAS NO MUSIC --------------------------------------------
//
// dScMgBase_c::AfterInitResources (vtable slot 2, src/minigames/d_s_mg_base.cpp,
// linked and in the slice) calls Mg_InitMusic on every minigame boot.
// That function:
//     if (gMgScene == 0) return;                 // scene tracker
//     if (Mg_GetMusicMode() == 0) {                     // enable at +0x2c
//         Sound::StopLoadedMusic_Layer1(1);                 // <-- NO MUSIC
//         if (gMgMenuRow != 0x16f) return;               // id at +0x00
//         Sound::LoadAndSetMusic_Layer1(Mg_GetMusicTrack());
//         return;
//     }
//     Sound::LoadAndSetMusic_Layer1(Mg_GetMusicTrack()); // enable != 0
// Mg_GetMusicTrack returns self->unk_0c8, which slot 1
// (src/minigames/d_s_mg_base.cpp:41) fills from gMgMenuRow + 0x28 (the music
// id). The base ctor _ZN11dScMgBase_cC2Ev:38 sets gMgScene = self,
// so the scene-tracker gate passes on a direct scene boot, and also reads
// gMgMenuRow + 0x30.
//
// So the whole music start is data-driven off ONE record, and two things about
// that record were wrong for a direct SM64DS_SCENE boot. (A THIRD defect, not
// about the record at all, is in hal/scene_boot.cpp: port_scene_tick never
// ticked the hosted ARM7, so even a correctly queued START was never drained.
// Measured -- with the record fixed and no per-frame tick, the framework's own
// gate answered enable=2 and music=12 and STILL no command reached the
// sequencer. Both halves are needed and both are in this lane.)
//
//   UNDERSIZED. The record is gMgMenuRow (span 0x14 to gMgMenuRowMid) plus
//   gMgMenuRowMid (span 0x20 to data_0209b33c) -- 0x34 bytes read as one
//   object (music at +0x28, enable at +0x2c, base-ctor field at +0x30). It was
//   hosted as `int gMgMenuRow[4]` (16 bytes) in
//   hal/actor_classes_ov081.cpp, where it landed only as the sibling of
//   gMgScene; every read past +0x0f was out of bounds. It is
//   re-hosted here at 0x34, using the packed-.dsstate idiom
//   hal/scene_boot.cpp:559 already uses for the run above it (the two symbols
//   have to be contiguous because the reads index one base across both), with
//   a run-time pack check. It stays in .dsstate so a save state captures it,
//   and it stays BSS (zeroed) because that is its DS boot value.
//
//   UNSEEDED. The only writer of the record is the Rec Room overlay ov005
//   (src/UpdateGameSelect.cpp:96-107), which copies a 0x34-byte row out of
//   sMiniGmRows for the minigame the player picked and THEN launches
//   the minigame scene. ov005 is in no slice and no mount, so a direct
//   SM64DS_SCENE=374 boot never runs it: music id and enable read zero, enable
//   0 takes the StopLoadedMusic branch, id 0 fails the != 0x16f test, and no
//   sound command is ever queued. port_scene_mg_seat_sound below copies the
//   same ROM row ov005 would have, keyed by scene id, before the scene spawns.
//
// ---- 2. THE SEED IS REAL ROM DATA, NOT AN OVERRIDE ------------------------
//
// The seed is the ov005 table verbatim, so it is north-star faithful and it
// generalises: seat any of the thirty minigame scenes and its music lights up
// with no edit here, the same way hal/scene_boot.cpp's port_scene_classes
// table lights up the debug-menu picker. The alternative considered was an
// SM64DS_SND_MUSIC-shaped env override (test-only, one scene at a time); it is
// not used because the table read is clean and the ROM row is the true answer.
// Curling's SND_MUSIC=12 chain (hal/sdat/consumer.cpp) and course boots
// (hal/star_flow.cpp:223) already prove Sound::LoadAndSetMusic_Layer1 plays,
// so seeding the record is the whole of what was missing.
//
// sMiniGmRows: overlay_0005.bin (base 0x020bfec0), file offset 0x2618,
// 0x750 bytes = 36 rows of 0x34, next symbol sMiniGmGroupTabs. Row layout
// (UpdateGameSelect.c's Entry): u16 id at +0x00, music at +0x28, enable at
// +0x2c. The lookup is first-match by id; that is exact for 29 of the 30
// minigames. Only id 0x017c appears twice with different music (row 15 =
// music 8 enable 1, row 35 = music 7 enable 2): the ROM's ov005 selects by
// menu POSITION, not by id, so the port cannot reproduce that split without
// ov005's touch logic and takes the first row. Curling (0x176) is unique:
// row 16, music 12 (SSEQ 12 = NCS_BGM_MINISNOW2), enable 2.
//
// The rows are read from the ov005 mount below, not carried here; at the
// player's ROM they are overlay_0005.bin file offset 0x2618, 0x750 bytes.
//
// See port/minigame_music.txt for the full derivation and the before/after
// sound-command trace.

#include <cstdio>
#include <cstring>

#include "dsstate_seg.h"

// ---- the re-hosted 0x34 record --------------------------------------------
//
// Two symbols, contiguous, at the ROM's 0x14 spacing. $mg0 then $mg1 sort
// between the .dsstate$aaa/$zzz sentinels so both land in the captured span,
// and adjacently so gMgMenuRow + 0x28 reaches into gMgMenuRowMid the way
// the reads above do. align(4) keeps the base word-aligned for the int reads;
// the pack check proves the spacing at run time rather than trusting it.
#pragma section(".dsstate$mg0", read, write)
#pragma section(".dsstate$mg1", read, write)
extern "C" {
__declspec(allocate(".dsstate$mg0")) __declspec(align(4))
unsigned char gMgMenuRow[0x14];
__declspec(allocate(".dsstate$mg1")) __declspec(align(4))
unsigned char gMgMenuRowMid[0x20];
}

static void port_mg_pack_check(void)
{
    long d = (long)(gMgMenuRowMid - gMgMenuRow);
    if (d != 0x14)
        std::fprintf(stderr, "  [mg-snd] PACK BROKEN: gMgMenuRowMid at +%ld, "
                     "ROM says +0x14\n", d);
}

// ---- the ov005 sound table: the ROM's own rows ----------------------------
// sMiniGmRows comes from the ov005 mount (port/ov005_syms.txt, emitted by
// tools/ovdata.py out of the player's overlay_0005.bin; a ROM-clean build fills
// it from romdata.bin at the top of main()). The mount packs the ROM's symbols
// contiguously, so the 36 rows run on from sMiniGmRows through
// sMiniGmSaveTable and the rest of each row to sMiniGmGroupTabs. The rows are
// only read through port_mg_sound_rows(), which proves that span first, the way
// hal/scene_mg_menu.cpp's mgm_pack_check proves it for the menu's own seat.
extern "C" unsigned char sMiniGmRows[];        /* the 36-row launch table */
extern "C" unsigned char sMiniGmGroupTabs[];   /* the symbol after it     */

static const unsigned char *port_mg_sound_rows(void)
{
    const long dt = (long)(sMiniGmGroupTabs - sMiniGmRows);
    if (dt != 36 * 0x34) {
        std::fprintf(stderr, "  [mg-snd] PACK BROKEN: the ov005 launch table "
                     "is %ld bytes, the ROM says %d (36 rows of 0x34)\n",
                     dt, 36 * 0x34);
        return 0;
    }
    return sMiniGmRows;
}

extern "C" int IsMinigameActorID(unsigned id);   /* src/IsMinigameActorID.c */

// ---- THE ROW'S OTHER HALF: THE SPAWN PARAMETER (run mg8, lane MMD) --------
//
// The rows above are not only a sound table. UpdateGameSelect -- the Rec
// Room's launch path, the same body that copies the 0x34 bytes into
// gMgMenuRow -- ends with
//
//     Scene::StartSceneFade(*(u16*)(sMiniGmRows + m),      the scene id
//                           *(int*)(sMiniGmSaveTable + m),      THE PARAM
//                           0);
//
// and sMiniGmSaveTable is sMiniGmRows + 4, so the SECOND argument is
// the row's own +0x04 word. Scene::StartSceneFade hands it to
// Scene::SetSceneToSpawn(id, param) and the spine lands it on the actor at +8.
//
// dScMgBase_c's constructor (src/_ZN11dScMgBase_cC2Ev.cpp) is what unpacks it:
//
//     *(short*)(self+0x465e) = sMgSceneKindByGame[(*(u32*)(self+8) >> 0x10) & 0xff];
//     *(int*)(self+8)        = *(u32*)(self+8) & 0xffff;
//
// and Mg_GetSaveTable reads (self->field_8 & 0xff00) >> 8 -- which the mask
// above deliberately keeps. So ONE word carries two indices:
//
//     (param >> 8)  & 0xff   the row of data_0209caf4, the PERSISTENT
//                            per-minigame save record. Mg_GetSavedRecord1
//                            reads field 1 of it and dScMgMemory2_c's
//                            InitResources copies that into +0xb4, which is
//                            the only input _ZN14dScMgMemory2_c12ShuffleCardsEv has for
//                            dealing 16, 18 or 20 cards.
//     (param >> 16) & 0xff   the row of sMgSceneKindByGame, the minigame's
//                            name-text id, read back at +0x465e by
//                            src/_ZN11dScMgBase_c17SaveScreenForMenuEv.cpp.
//
// hal/scene_boot.cpp used to pass 0 here, so every minigame in the port shared
// save record 0 and every one of them asked for name text index 0. Memory
// Master is row 29, param 0x001d1d01: record 29, text 29.
//
// FIRST MATCH BY ID, the same rule and the same caveat as the sound seat above
// -- six ids appear on two rows because the ROM's menu selects by POSITION.
// 0x016b is not one of them.
extern "C" unsigned port_mg_scene_spawn_param(int scene_id)
{
    if (scene_id < 0 || !IsMinigameActorID((unsigned)scene_id))
        return 0;
    const unsigned char *rows = port_mg_sound_rows();
    if (!rows)
        return 0;
    for (int i = 0; i < 36; ++i) {
        const unsigned char *row = rows + i * 0x34;
        unsigned id = (unsigned)row[0] | ((unsigned)row[1] << 8);
        if ((int)id == scene_id)
            return (unsigned)row[4] | ((unsigned)row[5] << 8)
                 | ((unsigned)row[6] << 16) | ((unsigned)row[7] << 24);
    }
    return 0;
}

// Seed the record with the ov005 row for scene_id, before the scene's base
// ctor reads it. A no-op for non-minigame scenes (the record stays at its DS
// boot value, zero). Called from hal/scene_boot.cpp's port_scene_begin, after
// the sdat_host_tick that seats the SDAT root the framework's own
// LoadAndSetMusic_Layer1 walks, and before port_scene_boot spawns the scene.
extern "C" void port_scene_mg_seat_sound(int scene_id)
{
    port_mg_pack_check();
    if (scene_id < 0 || !IsMinigameActorID((unsigned)scene_id))
        return;
    const unsigned char *rows = port_mg_sound_rows();
    if (!rows)
        return;
    for (int i = 0; i < 36; ++i) {
        const unsigned char *row = rows + i * 0x34;
        unsigned id = (unsigned)row[0] | ((unsigned)row[1] << 8);
        if ((int)id == scene_id) {
            std::memcpy(gMgMenuRow, row, 0x14);
            std::memcpy(gMgMenuRowMid, row + 0x14, 0x20);
            int music  = (int)((unsigned)row[0x28] | ((unsigned)row[0x29] << 8)
                             | ((unsigned)row[0x2a] << 16)
                             | ((unsigned)row[0x2b] << 24));
            int enable = (int)((unsigned)row[0x2c] | ((unsigned)row[0x2d] << 8)
                             | ((unsigned)row[0x2e] << 16)
                             | ((unsigned)row[0x2f] << 24));
            std::fprintf(stderr, "[mg-snd] scene 0x%x ov005 row %d: music=%d "
                         "enable=%d (%s)\n", scene_id, i, music, enable,
                         enable ? "start" : "silent");
            return;
        }
    }
    std::fprintf(stderr, "[mg-snd] scene 0x%x: no ov005 row, record left "
                 "zeroed\n", scene_id);
}
