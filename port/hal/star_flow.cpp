// Gate 35: the course loop -- what makes a level a GAME rather than a walk.
//
// Everything here is a SEAM onto matched src, not a reimplementation. The
// game already owns damage, death, respawn, the coin counter and the star
// bookkeeping; all of it is in the link and has been since gate 10. What was
// missing is the state those functions read before they mean anything, and a
// caller for the two or three entry points nothing on the port reaches yet.
//
// Four pieces:
//
//   port_course_seat()      the boot: player globals, the sound group/bank,
//                           and the course's own music, all through the
//                           game's own functions.
//   Actor::GivePlayerCoins  the COIN SEAM. Matched src, added to the link
//                           here. See the header over port_give_player_coins.
//   port_star_collect()     the star's bookkeeping and the course-clear
//                           handoff, which is the level-independent half of
//                           func_ov002_020e8ef0 plus func_ov002_020c94a4's
//                           _Z9ExitLevelv tail.
//   _Z13HitDeathPlanei           RETIRED at run link100 wave 15: main fixed the
//                           argument the host copy used to name, and the
//                           matched TU runs. See the block below.
//
// plus a probe surface (port_course_probe_*) the harnesses drive so every
// one of those paths can be shown moving with a log rather than asserted.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "sdat/sdat.h"

/* run mp1 lane STARREF1: the arena half of this file (the referee decides
   stars, hits, the clock and the winner); every use below is gated on the
   arena being installed with a referee. */

extern "C" {

// ---- the game's own functions this file calls ------------------------------
void SetPlayerGlobals(void);
int  _Z15GetSoundGroupIDii(int level, int sub);
void _ZN5Sound19LoadGroupAndSetBankEii(int group, int bank);
void _ZN5Sound22LoadAndSetMusic_Layer1Ei(int seqId);
unsigned int port_sf_sound_play2d(unsigned int bank, unsigned int id);
unsigned int func_02012790(unsigned int id);
void _ZN5Event6SetBitEj(unsigned int bit);
int  _ZN5Event6GetBitEj(unsigned int bit);
void _ZN5Event8ClearBitEj(unsigned int bit);
void _Z21CollectStarInCurLeveli(int starId);
int  _Z25IsStarCollectedInCurLeveli(int starId);
void SetNextLevel(int reason);
void StartExitCharacterWipe(void);
void StartExitFaderWipe(int wipeType);
void _Z9ExitLevelv(void);
void KillPlayer(void);
void GiveCoins(int playerNo, int amount);
short _Z8NumCoinsv(void);
unsigned char _Z8NumStarsv(void);
int  _ZN7daPly_c9GetHealthEv(void *player);
int  _ZN7daPly_c4HealEi(void *player, int amt);
int  _ZN7daPly_c4HurtERK7Vector3jihhh(void *self, void *src,
                                             unsigned kind, int knockback,
                                             unsigned char scaleByChar,
                                             unsigned char special,
                                             unsigned char particles);
void _ZN8dActor_c15GivePlayerCoinsER7daPly_chj(void *actor, void *player,
                                           unsigned char count,
                                           unsigned int kind);
int  _ZN7daPly_c19func_ov002_020d82f0Ev(void *player);   /* daPly_c::Hurt's own entry gate */
void sdat_host_tick(void);
int  _Z19IsLevelInsideCastlei(int level);           /* seat_course_sound's two   */
int  _Z28IsLevelTinyHugeIslandOutsidei(int level);  /* same-area music guards    */
void _ZN5Sound22StopLoadedMusic_Layer1Ej(unsigned int frames);
void _ZN5Sound8SetMusicEjj(unsigned int player, unsigned int seqId);
void _ZN5Sound10PauseMusicEv(void);            /* Stage::PS_Init's own call */

// ---- the globals it reads and seats ----------------------------------------
extern int          gRunningKuppaScript;    /* the running cutscene script, 0 = none */
extern int          gCurrentMusicID;    /* layer 1's current song (in .dsstate)   */
extern int          sLayer1Music;    /* layer 1's loaded base song (romdata,
                                         outside .dsstate)                      */
extern signed char  gCurrentSublevel;    /* current sublevel */
extern signed char  gNumLives[];  /* lives */
extern short        gPlayerHealth[];  /* per-player health: HP<<8 | fraction */
extern signed char  data_02092114;    /* QUEUED character swap; -1 = none */
extern signed char  sCurrentBank;    /* the SFX bank _Z22Player_PlaySoundEffectP12NNSSndHandleii uses */
extern unsigned char sLoadedGroup;   /* the sound group currently loaded */
extern unsigned char sPlayerVoiceGroup;
extern unsigned char gSoundInitialized;   /* master "sound effects on" */
extern unsigned char gCurrentStarId;   /* star filter */
extern short        gNumCoins[];  /* per-player coin counter */
extern unsigned char gCollectedStarId;   /* the star just collected */
extern unsigned char gNewStarCollected;   /* 1 = it was NEW */
extern unsigned char gLocalPlayerId;   /* local player index */
extern unsigned char gNextSublevel;   /* next sublevel  (_Z12SetNextLeveli) */
extern unsigned char sNextEntranceId;   /* next entrance  (_Z12SetNextLeveli) */
extern unsigned char sReturnState;   /* why we are leaving */
extern void        *gPlayers[];  /* per-player Actor* */
/* Sound::PlayLong, the game's own "start or refresh a looping sound", and its
   0x40-entry handle table. The loop probe drives PlayLong at the player's own
   camera-space position (which the listener is centred on, so it clears the 3D
   distance cull) and reads the table to prove the level-change reap emptied it. */
extern unsigned int _ZN5Sound8PlayLongEjjjRK7Vector3s(unsigned int j1,
    unsigned int j2, unsigned int j3, void *v, unsigned int j5);
extern int data_0209b53c[];   /* PlayLong's handle table: 8-byte header, 0x40
                                 entries of 0x14; live when entry+0 != 0 */
extern unsigned char data_0209caa0[];
/* {group, bank, bgm} x 0x34, from romdata. config names the three columns
   separately (data_02075768/69/6a) because the ROM walks them with a stride
   of three; the port emits the run once and does the same arithmetic. */
extern unsigned char data_02075768[];
/* The VS half of the stage's state. Spellings are the ones the two ROM
   functions this file stands in for use: Stage::InitResources declares
   data_0209f2bc s8 and data_0209f304/f308 s16, Stage::Behavior declares
   data_0209f2bc u8 and data_0209f304 u16; the host storage is four bytes wide
   either way (hal/w8a_stage_storage.cpp, hal/auto_bss.cpp). */
extern unsigned char gGameMode;   /* game mode: 0 adventure, 1 VS, 2 script */
extern unsigned char data_0209f2bc;   /* VS start countdown: 3, 2, 1, 0 */
extern unsigned short data_0209f304;  /* frames left in the current count */
extern short         data_0209f308;
extern unsigned char gGamePaused;   /* pause-screen state; 0 = playing */
extern unsigned char data_0209fc50;   /* players in the match (SetNumPlayers) */
extern int           data_0208ee44;   /* vblanks per game tick */
extern unsigned char gMainScreenLayers;   /* engine A's software layer mask; the
                                         frame's DISPCNT publish copies it */

}  // extern "C"
#pragma comment(linker, "/alternatename:_port_sf_sound_play2d=?Play2D@Sound@@YAIII@Z")  /* catchup1 round 2: the ROM name _ZN5Sound6Play2DEjj is the C++ overload in src */

// =============================================================================
// _Z13HitDeathPlanei: RETIRED, the matched TU runs (run link100 wave 15, SEAT15F)
// =============================================================================
//
// This file used to carry a host copy of _Z13HitDeathPlanei, because src/ declared
// `extern void _Z12SetNextLeveli(void)` and called it with no arguments while
// src/SetNextLevel.c defined `_Z12SetNextLeveli(int arg)`: right on ARM, where the
// value is already in r0 and the ROM's `bl` never touches it, and wrong on
// cdecl, where `arg` read whatever the caller's stack happened to hold -- and
// `arg` is what lands in sReturnState, the reason-for-leaving code the next
// scene reads.
//
// MAIN HAS SINCE FIXED IT. src/HitDeathPlane.c at 8ddff3187 declares
// `extern void _Z12SetNextLeveli(int arg);` and calls `_Z12SetNextLeveli(arg)`, and its
// other two callees (StartExitFaderWipe, dScene_c::StartSceneFade) match this
// file's own declarations of them argument for argument. So the host copy was
// a stand-in for a defect that no longer exists, and it is gone: the matched TU
// is back in SLICE10_CAM_SOURCES (the filter line in port/CMakeLists.txt came
// out with the body) and the ROM's own text runs on the death-plane path.
//
// The one declaration the body left behind is kept: the VS fade below still
// calls dScene_c::StartSceneFade.
extern "C" {
void _ZN8dScene_c14StartSceneFadeEjjt(unsigned a, unsigned b, unsigned short c);
}  // extern "C"


// =============================================================================
// The boot: the state the whole loop reads before any of it means anything
// =============================================================================

namespace {

int g_seated;
int g_course_music = -1;
/* seat_course_sound's prevLevel: gCurrentSublevel as it stood when the port's
   latch replaced it (hal/level_change.cpp port_level_latch, the ROM's :226).
   -1 until the first latch; the cartridge's bss zero is not a castle level
   either. */
int g_music_prev_level = -1;

/* What seat_course_sound worked out for the level music, kept for
   seat_course_music: the ROM makes the music call later in
   Stage::InitResources than the sound group / bank statements, so the two
   halves run at two points of the boot. */
int g_boot_group, g_boot_bank, g_boot_vs, g_boot_bgm;

/* THE QUEUED SWAP TRAP. data_02092114 is the in-level character swap request
   and -1 means "nothing queued"; SetPlayerGlobals is the only thing that ever
   writes that -1. The port left the symbol zero-initialised, which reads as a
   pending swap to character 0. It is inert while the local player IS Mario
   (daPly_c::InitResources takes the same branch either way), which is why the
   port has walked around for eleven gates without noticing -- and it stops
   being inert the moment anything queues a real swap or checks `>= 0`.
   Running the game's own function is the fix, and it seats the health words
   and the life count in the same call. */
void seat_player_globals(void)
{
    SetPlayerGlobals();
    fprintf(stderr, "[course] player globals: hp=%d/%d lives=%d "
            "queued-swap=%d\n", gPlayerHealth[0] >> 8, 8,
            (int)gNumLives[0], (int)data_02092114);
}

/* The course's sound row, exactly as Stage::InitResources reads it:

       soundGroup = _Z15GetSoundGroupIDii(level)      -- data_02075768[level*3]
       bank       = data_02075769[level*3]      -- ...[level*3 + 1]
       Sound::LoadGroupAndSetBank(soundGroup, bank)
       Sound::LoadAndSetMusic_Layer1(data_0207576a[level*3])

   THE BANK IS WRITTEN HERE AND THAT IS DELIBERATE. LoadGroupAndSetBank is
   matched src and in the link, but its first line is func_0203d974, whose
   read at 0x027ffc40 the port answers for. (This note used to say that
   address was outside every region ntr maps. It is not: SHARED_BASE
   0x027ff000 is a fatal region in ntr/io.cpp's kRegions, and the port writes
   a cartridge-boot 0 there. The reason the face stays is the sound heap, not
   the mapping -- see func_0203d974 below.) The port answers 1 and the ROM's `1`
   branch returns early for any group but 0x2f without recording anything.
   Both branches END by storing the bank and the group, and those two words
   are the whole observable effect once the load itself is a no-op (the SDAT
   consumer pre-seats every FAT residency slot, so there is nothing to load).
   So the port writes what the branch it did not take would have left behind,
   and says so, rather than editing matched src or faking the load.

   sCurrentBank matters concretely: _Z22Player_PlaySoundEffectP12NNSSndHandleii passes it as the
   BANK for every kind-3 sound, and a zero there forces bank 0 on sounds whose
   own SEQARC entry names a different one. */
/* THE VS ARENAS TAKE THE OTHER BRANCH, AND THE PORT WAS TAKING NEITHER.
 *
 * The block above transcribes only the ADVENTURE half of the ROM's sound row.
 * The ROM's own half it left out is the first thing InitResources does:
 *
 *     int soundGroup = 0;
 *     u8  bank = 0x36;
 *     if (gCurrentStarId == 2) {                       // a VS match
 *         if (level == 1 || level == 0x33) soundGroup = 0x2B;
 *         else if (level == 0x1D)          soundGroup = 0x2E;
 *         else if (level == 0x2A)          soundGroup = 0x2C;
 *         else if (level == 0x2B)          soundGroup = 0x2D;
 *     }
 *     if (soundGroup == 0) {                          // adventure: the table
 *         soundGroup = _Z15GetSoundGroupIDii(level);
 *         bank       = data_02075769[level * 3];
 *     }
 *     Sound::LoadGroupAndSetBank(soundGroup, bank);
 *
 * -- src/_ZN5Stage13InitResourcesEv.cpp:314-327, and those four ids are the
 * four VS arenas (51, 29, 42, 43). gCurrentStarId is 2 in a match because the
 * VS start stages it that way: _ZN10dScEntry_c10StartMatchEv calls
 * LoadLevelNoReturn(map, 0, 2, 0), _Z9LoadLevelijija puts that starID in gNextStarId
 * and InitResources copies it to gCurrentStarId.
 *
 * WHY IT IS NOT COSMETIC. `bank` is sCurrentBank, which _Z22Player_PlaySoundEffectP12NNSSndHandleii
 * hands to func_02051e60 as the SBNK every kind-3 sound is looked up in, and
 * func_02051a98 then resolves each note's PROGRAM in that bank. A program the
 * bank does not carry is not a wrong instrument, it is NO NOTE -- the note is
 * dropped where hal/sdat/sseq.cpp's start_note says "program %d has no note
 * there", without a sound and without an error.
 *
 * MEASURED, this branch off versus on: same binary, same 600-frame run, same
 * staged star, bank the only variable. Arena map 0 (level 51) under the
 * adventure bank 0x20: sound 0x39 lost 15 notes and sound 0x3a lost 3, all of
 * them to programs 22 and 23, which bank 0x36 carries and 0x20 does not --
 * under 0x36 both sounds come back whole. Arena map 3 (level 42) under
 * adventure bank 0x34: sounds 0x173, 0x40, 0x41 and 0x06 lost 15 notes between
 * them to programs 22 and 25, and all four come back. That is the reported
 * "some sounds do not play in the arena", and it is per-NOTE rather than
 * per-sound, which is why it reads as intermittent rather than as a sound
 * being missing outright.
 *
 * It is not a one-way trade and this comment will not pretend it is. Bank 0x36
 * has no program 6, so on levels 51 and 29 the coin VARIANT 0x12 loses its
 * program-6 track where the adventure bank happened to have one. That is what
 * the DS does with the bank the DS loads. The chime a normal pickup plays is
 * 0x11, whose program 19 bank 0x36 carries.
 *
 * Nothing here is invented: the branch, the four constants, the 0x36 default
 * and the fall-through to the table are the ROM's, and the adventure path
 * below is bit-for-bit what it was. */
static int vs_sound_group(int level)
{
    if (gCurrentStarId != 2)
        return 0;
    if (level == 1 || level == 0x33) return 0x2B;
    if (level == 0x1D)               return 0x2E;
    if (level == 0x2A)               return 0x2C;
    if (level == 0x2B)               return 0x2D;
    return 0;
}

void seat_course_sound(int level)
{
    const unsigned char *row = data_02075768 + level * 3;
    int group = vs_sound_group(level);
    int bank = 0x36;
    const int vs = (group != 0);
    if (!vs) {
        group = _Z15GetSoundGroupIDii(level, 0);
        bank = row[1];
    }
    /* SIGNED, and that is the whole meaning of the column: Stage::InitResources
       declares it `extern s8 data_0207576a[]`, and LoadAndSetMusic_Layer1's
       first branch is `if (j < 0) stop`. 0xff is "this sublevel has no layer-1
       track", not "sequence 255" -- reading it unsigned asks the sequencer for
       an entry 172 past the end of an 83-entry SSEQ table. */
    int bgm = (signed char)row[2];

    /* The SDAT root has to be seated before any of this: LoadAndSetMusic_Layer1
       walks data_020a5bb8 + 0x84 unconditionally, and unlike Sound::Play it
       does not self-initialise. One tick with nothing queued does it. */
    sdat_host_tick();

    _ZN5Sound19LoadGroupAndSetBankEii(group, bank);
    sCurrentBank = (signed char)bank;
    sLoadedGroup = (unsigned char)group;
    sPlayerVoiceGroup = 0;
    gSoundInitialized = 1;                 /* master SFX flag; the consumer's
                                          init sets it too, but the boot may
                                          run before the first sound call */

    /* SM64DS_COURSE_MUSIC=<seq> overrides the row. The castle grounds is
       sublevel 1 and its row really does say -1 -- arriving there STOPS
       layer 1, and what the player hears outside is the AMBIENT_SOUND_EFFECTS
       actors, not a BGM. So the one level the port can boot today is also the
       one that proves nothing about a course theme playing; this is how the
       real thing (Bob-omb Battlefield is sublevel 6, group 6, bank 0x21,
       sequence 58) gets exercised before that level mounts. */
    const char *ov = std::getenv("SM64DS_COURSE_MUSIC");
    if (ov) {
        bgm = std::atoi(ov);
        fprintf(stderr, "[course] SM64DS_COURSE_MUSIC=%d overrides the row\n",
                bgm);
    }

    g_boot_group = group;
    g_boot_bank = bank;
    g_boot_vs = vs;
    g_boot_bgm = bgm;
}

/* THE LAYER-1 MUSIC CALL, WHERE STAGE::INITRESOURCES MAKES IT.
 *
 * src/_ZN5Stage13InitResourcesEv.cpp:380-391: the music statement comes AFTER
 * Stage::LoadClsnAndObjects (the call that builds the level's actors, the
 * Player among them), and the sound group / bank statements that
 * seat_course_sound still makes up front come well before it. The port ran
 * both together, before the Player existed. That matters for the songs whose
 * variant follows the Player: LoadAndSetMusic_Layer1 ends in _Z13func_02011dccP12NNSSndHandlei,
 * which asks _Z13func_02013078v for the variant and applies it at once with
 * func_0204921c, and _Z13func_02013078v reads the Player (underwater flag, height)
 * and answers -1 when there is none. So the cartridge starts Jolly Roger Bay
 * and Dire Dire Docks (NCS_BGM_WATER, layer A = tracks 0-1, layer B = tracks
 * 2-13) with the variant for where Mario is already in place, and the port
 * started with every track at full volume (the upbeat mix) and only then let
 * func_020132d8 ramp the unwanted layers down over about a hundred frames.
 * Measured on both: the cartridge sends the INITMODE pair (volume 0 and a
 * mute on each layer) in the frame of the START; the port sent none. */
void seat_course_music(int level)
{
    const int group = g_boot_group, bank = g_boot_bank, vs = g_boot_vs;
    const int bgm = g_boot_bgm;

    /* THE LAYER-1 CALL IS NOT PART OF THE VS ROW, AND THE PORT WAS MAKING IT.
     *
     * The block above is InitResources' sound-GROUP statement, which the ROM
     * guards on `gGameMode != 2`; VS is mode 1, so it runs and the VS
     * branch above is right to be here. The MUSIC statement forty lines later
     * is a different statement with a different guard
     * (src/_ZN5Stage13InitResourcesEv.cpp:365-381):
     *
     *     int v0_8 = (gGameMode == 1) ? 1 : 0;
     *     if (v0_8 == 0) {
     *         ... four more preconditions ...
     *         Sound::LoadAndSetMusic_Layer1(data_0207576a[level * 3]);
     *
     * -- so a VS match reaches the table row NEVER. The port called it anyway,
     * and the arena's row is what came out:
     *
     *   level 51 row[2] = -1  ->  LoadAndSetMusic_Layer1's own `if (j < 0)`
     *                             STOP branch. Silence, and this is Tango's
     *                             "no music at all" on VS map 0.
     *   level 42 row[2] = 65  ->  the ADVENTURE track for the Battle Fort,
     *                             which the DS never plays in a match.
     *
     * The arena's real music is Stage::Behavior's, at the end of the 3-2-1 --
     * started from its VS block.
     *
     * THE OTHER FOUR PRECONDITIONS are what keeps the castle theme playing
     * from room to room (src/_ZN5Stage13InitResourcesEv.cpp:386-391):
     *
     *     if (gRunningKuppaScript == 0)                               no cutscene
     *     if (!_Z19IsLevelInsideCastlei(level) || !_Z19IsLevelInsideCastlei(prevLevel))
     *     if (!_Z28IsLevelTinyHugeIslandOutsidei(level) ||
     *         !_Z28IsLevelTinyHugeIslandOutsidei(prevLevel))
     *     if (level != 2 || (data_0209caa0[2] & 0x200))         castle 1F seen
     *         Sound::LoadAndSetMusic_Layer1(data_0207576a[level * 3]);
     *
     * So walking between two castle rooms (sublevels 2, 4, 5 and 0x32, all
     * row 57) or between the two Tiny-Huge Island outsides makes NO music
     * call and the song already playing carries on. Without them the port
     * restarted the castle theme at every door. Level 2 before event bit
     * 0x200 is the first castle entry: daPly_c::St_Talk_Cleanup starts 0x39
     * itself once the welcome message closes. prevLevel is gCurrentSublevel
     * before InitResources re-latches it (:226); the port's latch runs in
     * hal/level_change.cpp before this boot and hands the old value to
     * port_music_note_prev_level below. It is the global, not the last level
     * booted: ExitMinigameMenu writes 6 into it on the way back to the Rec
     * Room, so that return is not castle to castle and the castle song, which
     * the minigame menu stopped, starts again. */
    const int vs_mode = (gGameMode == 1);
    const int prev = g_music_prev_level;
    const char *skip = 0;
    if (vs_mode)
        skip = "VS: the ROM does not read this column";
    else if (gRunningKuppaScript != 0)
        skip = "a cutscene script is running: no call";
    else if (_Z19IsLevelInsideCastlei(level) && _Z19IsLevelInsideCastlei(prev))
        skip = "castle room to castle room: no call, the song carries on";
    else if (_Z28IsLevelTinyHugeIslandOutsidei(level) &&
             _Z28IsLevelTinyHugeIslandOutsidei(prev))
        skip = "Tiny-Huge Island outside to outside: no call, the song "
               "carries on";
    else if (level == 2 && !(((const int *)data_0209caa0)[2] & 0x200))
        skip = "first castle entry: no call, the welcome message starts it";

    /* g_course_music is only read by port_course_sound_probe, and it means
       "what this file last asked layer 1 for". In a match nothing is asked for
       here, and _ZN10dScEntry_c10StartMatchEv's StopLoadedMusic_Layer1 has already run,
       so -1 is the true state until the countdown starts 0x4d. A skipped
       adventure call asks for nothing, so the value stays as it was. */
    if (vs_mode)
        g_course_music = -1;
    else if (!skip)
        g_course_music = bgm;
    fprintf(stderr, "[course] sublevel %d sound row: group=%d bank=0x%02x "
            "bgm=%d (%s) [%s branch, star=%d, from sublevel %d]\n", level,
            group, bank, bgm,
            skip ? skip : bgm < 0 ? "no layer-1 track" : "start",
            vs ? "VS arena" : "adventure table", (int)gCurrentStarId, prev);
    if (!skip) {
        _ZN5Sound22LoadAndSetMusic_Layer1Ei(bgm);
        /* The ROM's next statement (src/_ZN5Stage13InitResourcesEv.cpp:
           391-395): the local player index, and when that Player's byte
           0x6ff is set (the cap-start sublevels) SetMusic(index, 0x33), the
           cap song over the row song. Sublevel 31 ends on it. */
        const unsigned char sndIdx = gLocalPlayerId;
        if (gPlayers[sndIdx] &&
            ((const unsigned char *)gPlayers[sndIdx])[0x6ff] != 0)
            _ZN5Sound8SetMusicEjj(sndIdx, 0x33);
    }
    /* Push the START the music call just queued at the ARM9 half through the
       hosted ARM7, so the first frame already has voices allocated rather
       than a batch sitting in the ring. */
    sdat_host_tick();
}

/* Stage::InitResources' VS COUNTDOWN SEAT, its lines 270-279:
 *
 *     if (gGameMode == 1) { data_0209f2bc = 3;      // 3, 2, 1
 *                               data_0209f304 = 0x28;   // frames per count
 *                               data_0209f308 = 0; }
 *     else                    { data_0209f2bc = 0;
 *                               data_0209f304 = 0; }
 *
 * port/stage_lifecycle_map.txt section 2d lists all three as ABSENT from the
 * port, with data_0209f2bc flagged as already having a live reader. They are
 * seated now because a second reader turned up and it is the one Tango
 * reported: with all three left at their bss zero, Stage::Behavior's countdown
 * cannot start, and the arena's music is the last thing that countdown does.
 * Nothing else in this file reads them; the tick below does. */
void seat_vs_countdown(void)
{
    if (gGameMode == 1) {
        data_0209f2bc = 3;
        data_0209f304 = 0x28;
        data_0209f308 = 0;
        fprintf(stderr, "[vsgo] VS countdown armed: %d counts of %d frames, "
                "step %d\n", (int)data_0209f2bc, (int)data_0209f304,
                data_0208ee44);
    } else {
        data_0209f2bc = 0;
        data_0209f304 = 0;
    }
}

/* ---- WHY THERE IS NO VS SCOREBOARD AND NO MATCH CLOCK ON SCREEN -----------
 *
 * Reported from real play: no star count and no countdown anywhere, on either
 * screen, for the whole match. The HUD is NOT missing and this is worth saying
 * first, because "the HUD actor never spawns" was the reasonable guess and it
 * is wrong. Measured, SM64DS_VS_HUD=1 (hal/input_probe.cpp) on a live arena:
 *
 *     [vshud] f3 DISPCNT_A 00000000 OBJ OFF | mask d45c=00 (engine A, OBJ NOT
 *             asked for) d454=18 | engine A OAM: 0 all-zero, 122 parked,
 *             6 PLACED
 *
 * Six sprites PLACED in engine A's OAM on frame 3 and every frame after. The
 * HUD is alive, HUD::Behavior runs (it is what counts the match clock down --
 * that is the same object), HUD::Render takes its VS arm, and RenderVsTimer
 * and RenderStarCount put their digits exactly where the ROM puts them: engine
 * A, the TOP screen, sub=0 on every OAM::Render call in both.
 *
 * WHAT IS MISSING IS ONE ASSIGNMENT. On the DS nothing writes DISPCNT's layer
 * enables directly. Code sets a software mask -- gMainScreenLayers for engine A --
 * and _Z13func_02019144v copies it into bits 8-12 once a frame. The port does that
 * copy (hal/message_compositor.cpp's engine-A publish, every frame, and it
 * works). What the port never does is SET the mask, because the line that sets
 * it is Stage::InitResources line 402:
 *
 *     gMainScreenLayers = 0x11;          BG0 and OBJ
 *
 * and Stage::InitResources is not run by the port. port/stage_lifecycle_map.txt
 * section 2d lists that exact statement with "none" in its seat column and
 * notes hal/auto_bss.cpp leaves the word zero. So the mask is 0, the publish
 * copies 0, and the engine-A compositor's `!any_bg && !obj_on` exit returns
 * before it draws -- with the HUD's sprites sitting in OAM the whole time.
 *
 * ---- THE ADVENTURE POWER METER NEEDS THIS SEAT, AND SO IT IS UNGATED --------
 *
 * The ROM's line has no condition and neither does the port's now. The gate was
 * VS-only for a while on a measurement that was right and a conclusion that was
 * wrong, and this is the lane the old note invited ("un-gating is a one-word
 * change for the lane that finds one").
 *
 * THE MEASUREMENT: with the seat forced on, adventure levels 1 and 5 render
 * BYTE-IDENTICAL to the un-seated frame, because engine A's OAM holds ZERO
 * placed sprites on those frames. THE WRONG CONCLUSION drawn from it was that
 * the adventure HUD "reaches the player through engine B, not this register"
 * and that the seat "is necessary for it and nowhere near sufficient". Both are
 * false. Engine A's OAM was empty because the POWER METER is the one adventure
 * HUD leaf that draws to engine A (HUD::RenderHealthMeter calls OAM::Render with
 * sub=0, main engine, and GX::LoadOBJPltt on the main palette), and the power
 * meter is RETRACTED off-screen until Mario takes damage -- mHealthMeterY sits
 * negative, so OAM::Render culls every one of its nine sprites and places
 * nothing. A boot selftest that never gets hurt is measuring a HUD element that
 * is deliberately hidden.
 *
 * DEPLOY IT AND THE SEAT IS EXACTLY SUFFICIENT. Drive one daPly_c::Hurt on level
 * 1 (SM64DS_COURSE_PROBE=hurt) and the meter slides in at x=0x80, top centre:
 * with the seat its nine sprites are PLACED in engine A's OAM and the engine-A
 * compositor draws the "POWER" word and the segmented pie over the 3D frame,
 * exactly where the DS puts it and one wedge down; without the seat the same
 * hit still drops HP (the game logic is untouched by this register) but the
 * meter never reaches the picture, because the OBJ-layer bit the compositor
 * gates on is never set. Coins, stars and the map still reach engine B and the
 * bottom-screen inset as before -- this changes only the one leaf the ROM
 * itself routes to the main engine.
 *
 * SO THE SEAT IS UNCONDITIONAL BY DEFAULT. It is the ROM's own statement and it
 * is what makes the health meter visible in ordinary play. The env knob keeps
 * the older behaviours for A/B: SM64DS_HUD_LAYER_SEAT=off restores the pre-seat
 * frame, =vs restores the VS-only gate, =always is the default spelled out.
 *
 * THE NAME IS THE SECOND ONE THIS KNOB HAS HAD, and the first was a bug I
 * shipped. It was SM64DS_ENGINE_A_LAYERS, which is ALREADY a live variable:
 * hal/message_compositor.cpp reads it as a HEX LAYER SUBSET
 * (strtoul(e, 0, 16) & 0x1F, default 0x1f), port/tools/headroom.py sets it, and
 * port/ppu_gap_audit.txt documents it. Under that grammar the documented value
 * "always" parses as 0x0a -- BG1 and BG3, with OBJ DROPPED, which is the layer
 * the entire HUD lives on -- and "off" parses as 0x00. Review measured both
 * rendering byte-identical while this seat's own log line said the mask had
 * been seated: the mask was seated and then thrown away downstream. Renamed,
 * and the compositor's variable is untouched.
 *
 * AND THE PREVIEW =always PROMISES IS EMPTY ON THE LEVELS MEASURED, which is a
 * correction to this lane's own round-1 open question rather than a defect in
 * the knob. Re-measured after the rename, adventure selftests on levels 1 and
 * 5, SM64DS_HUD_LAYER_SEAT=always against =off: the seat fires (its log line
 * prints, DISPCNT_A goes 0x00000008 -> 0x00001108, OBJ ON) and the frame is
 * still BYTE-IDENTICAL -- because engine A's OAM holds 0 PLACED sprites on both
 * levels. The adventure HUD draws to the SUB screen: HUD::Render's adventure
 * arm runs (measured, sub OAM 237 nonzero bytes) and every leaf it reaches
 * there goes to engine B. So "the adventure HUD is one statement away" was
 * WRONG. This seat is necessary for it and nowhere near sufficient, and the
 * lane that takes it inherits finding out what actually submits to engine A on
 * an adventure screen. */
void seat_engine_a_layers(void)
{
    static int mode = -1;   /* 0 off, 1 VS only, 2 unconditional (default) */
    if (mode < 0) {
        const char *e = std::getenv("SM64DS_HUD_LAYER_SEAT");
        mode = 2;
        if (e && std::strcmp(e, "off") == 0) mode = 0;
        else if (e && std::strcmp(e, "vs") == 0) mode = 1;
        else if (e && std::strcmp(e, "always") == 0) mode = 2;
    }
    if (mode == 0)
        return;
    if (mode == 1 && gGameMode != 1)
        return;
    gMainScreenLayers = 0x11;
    /* AND BG0 IS THE 3D LAYER, WHICH HAS TO BE SAID OR THE MASK MAKES IT WORSE.
     *
     * DISPCNT bit 3 selects whether engine A's BG0 shows the 3D engine's output
     * or an ordinary tiled background.
     *
     * AND THE ROM SETS IT IN THIS VERY FUNCTION, 151 lines above the mask
     * statement and in the same unconditional block (found in review; an
     * earlier draft argued this from hardware behaviour, which was true and
     * weaker):
     *
     *     src/_ZN5Stage13InitResourcesEv.cpp:251
     *         int one = 1;
     *         GX::SetGraphicsMode(one, 0, one);      // c = 1
     *
     * and GX::SetGraphicsMode's body is `reg = (c << 3) | reg`
     * (src/_ZN2GX15SetGraphicsModeEiii.cpp), so c = 1 IS bit 3. The two lines are
     * one statement of the ROM's, split across the function: turn BG0 into the
     * 3D layer, then name BG0 in the layer mask. Seating the mask without the
     * bit takes half of it.
     *
     * The port never calls SetGraphicsMode: it renders 3D through its own path
     * and leaves engine A's DISPCNT alone, so the register's low byte is zero
     * for the whole run.
     *
     * hal/message_compositor.cpp keys on that bit by name: BG0 with bit 3 set
     * is "the 3D LAYER, not composited here (the 3D frame is already in the
     * framebuffer)", and without it an ordinary 2D text background. So seating
     * the mask ALONE would enable a BG0 the DS never composites as 2D, and the
     * compositor would draw whatever happens to be in that tilemap over the
     * whole top screen. Measured, before this line was added: 34638 changed
     * pixels spread over every band of the top screen instead of the HUD's own
     * rows. With it, the compositor skips BG0 exactly as the hardware does and
     * only OBJ reaches the picture. (An earlier revision of this line said
     * "the timer and the star count". It was the timer alone, and the next
     * block is why.)
     *
     * A raw register write in a host file is not the shape this tree likes, and
     * calling the ROM's own SetGraphicsMode instead is worse: its `a` and `b`
     * arguments also set the display mode and the BG mode, two fields the port
     * deliberately leaves at zero because it does not drive engine A. One bit,
     * which is the one bit of that call the port can honour.
     *
     * "THE PORT NEVER CALLS SetGraphicsMode" IS HISTORY ON THE LEVEL PATH (run
     * linkfull, lane HUDQUIT1). hal/level_boot.cpp's port_stage_boot_body now
     * makes Stage::InitResources' own first call, dScene_c::Initialise3dGraphics,
     * on every Stage build, and that function calls GX::SetGraphicsMode(1, 0,
     * 1) after its register reset: the ROM sets bit 3 itself, and the
     * display-mode and BG-mode fields it writes are its own values. The store below therefore finds the bit set
     * and changes nothing on a level boot; it is kept because it is harmless,
     * and retiring it is a separate change. The layer mask and the BG0CNT
     * priority are still this seat's alone. */
    *(volatile unsigned int *)0x04000000 |= 8;
    /* AND THE 3D LAYER'S PRIORITY, WHICH IS WHY HALF THE VS HUD WAS INVISIBLE.
     *
     * Found chasing the coin move (round 2, order 1) and it is a bigger bug
     * than the thing that found it: THE VS STAR COUNT HAS NEVER RENDERED in
     * this port. Measured with the OAM census, engine A, a live arena:
     *
     *   OAM[0..2]  the timer      prio 0   -> drawn
     *   OAM[3..5]  the star count prio 1   -> placed, real tiles, NOT drawn
     *
     * hal/message_compositor.cpp's layer_behind_3d is the ROM's own rule --
     * a sprite loses to the 3D layer only when `prio > p3d` -- and p3d is read
     * straight out of BG0CNT (0x04000008 bits 0-1). The port never writes that
     * register, so it reads 0, every priority-1 sprite is strictly worse than
     * the 3D layer, and the compositor drops it. The rule is right; the input
     * to it was never seated.
     *
     * THE VALUE IS 2, FROM THE LEVEL PATH'S OWN Stage::InitResources:330:
     *
     *     *(vu16 *)0x04000008 = (*(vu16 *)0x04000008 & ~3) | 2;
     *
     * in the same unconditional block as line 251's GX::SetGraphicsMode(1, 0, 1)
     * and line 402's layer mask -- the two statements this seat already hosts.
     * Nothing on the level path writes engine A's BG0CNT after it:
     * Stage::LoadGraphics2D, which runs two lines later, touches BG0CNT_SUB
     * (0x04001008) only. So all three lines are one statement of the ROM's,
     * split across the function, and this seats the third.
     *
     * AN EARLIER REVISION WROTE 1 AND CITED src/_ZN9BootScene13InitResourcesEv.cpp FOR IT, calling
     * that "not a guess". Both halves were wrong and review caught them.
     * _ZN9BootScene13InitResourcesEv is dScBoot_c::InitResources -- a BOOT scene, not the level
     * path -- and its header carries "recovered from vtable slot identity",
     * which is the GUESSED-BODY marker. This lane dropped a body in round 1 for
     * carrying exactly that marker and then quoted another one as authority in
     * the same breath. It cost no pixels (a sprite clears `prio > p3d` at either
     * value, so every measurement taken under the wrong one still stands) and it
     * was still a seated hardware statement with a wrong value and a false
     * provenance: it diverges for BACKGROUNDS, whose rule is `prio >= p3d`, and
     * it printed into every VS playlog.
     *
     * Seating the two priority bits, and nothing else in that register, is the
     * smallest statement of the ROM's own fact. It is what makes the star count
     * appear, and it is the precondition for the coin move in
     * hal/sub_actors.cpp -- the coins are priority 1 too. */
    *(volatile unsigned short *)0x04000008 =
        (unsigned short)((*(volatile unsigned short *)0x04000008 & ~3) | 2);
    fprintf(stderr, "[vshud] engine A layer mask seated: gMainScreenLayers = 0x11 "
            "(BG0 + OBJ), DISPCNT bit 3 (BG0 is the 3D layer) and BG0CNT "
            "priority 2 (Stage::InitResources:330's own value, which is what "
            "lets a priority-1 HUD sprite draw over the arena). %s\n",
            mode == 2 ? "unconditional: every mode (default; SM64DS_HUD_LAYER_SEAT=off/vs to change)"
                      : "VS only (SM64DS_HUD_LAYER_SEAT=vs)");
}

}  // namespace

extern "C" {

/* func_0203d974, hosted. Removed from slice_gate14's source list.
 *
 * The ROM reads the BOOT INDICATOR the firmware leaves at 0x027ffc40 -- "was
 * this console download-played" -- and ORs it with "is my comms slot not the
 * parent's". (An earlier draft of this note called it a console-type word and
 * said nothing mapped it. Both halves are wrong now: ntr/io.cpp maps
 * SHARED_BASE 0x027ff000 as a fatal region, and the port writes 0 there, a
 * cartridge boot. See the boot-indicator note at that write.) So the read is
 * fine and the DS-faithful ANSWER is 0 -- which is exactly the branch this
 * face exists to avoid, for the reason below.
 *
 * The port answers 1. Not because the host is a DSi, but because the 0 branch
 * is the one that walks the SOUND HEAP -- _Z13func_020134d8PvP7SndHeap -> func_02051918 over
 * sSndHeap, and func_0205117c's SolidHeapAllocator save/restore. That
 * heap is built by func_02050f34, which hal/sdat/consumer.cpp SKIPs at init
 * (hal/sdat/sdat.cpp seats an equivalent root instead), so sSndHeap is
 * null and the DS-faithful answer is a null dereference three calls down. The
 * 1 branch is a clean early return for every group the game asks for.
 *
 * What that costs: nothing on the port, because there is nothing to load.
 * What it hides: the bank/group store at the end of both branches, which
 * seat_course_sound writes instead. When the sound heap is built for real
 * (see the report), this returns to the src version and that store goes away.
 * PORT_HOST_ABI: src reads the unmapped console-type halfword at 0x027ffc40.
 */
int func_0203d974(void)
{
    return 1;
}

/* The sound row, run FROM THE BOOT at the point Stage::InitResources runs it.
   InitResources seats the course's sound group/bank and layer-1 music between
   its archive load and LoadClsnAndObjects (the block around lines 311-328 and
   365-381 of src/_ZN5Stage13InitResourcesEv.cpp), driven by _Z15GetSoundGroupIDii,
   Sound::LoadGroupAndSetBank and Sound::LoadAndSetMusic_Layer1 -- the same
   matched functions seat_course_sound calls. Hosting it here, called from
   port_stage_a_boot, is what lets the sound row ride EVERY boot the way the ROM
   does, the warp included, instead of only the one gate-35 seat that ran once
   per process. Per-boot guard, reset by the level change, so a warp re-seats.
   The one non-matched write is the bank/group store the hosted func_0203d974==1
   early return skips (see seat_course_sound's own note). */
void port_boot_course_sound(int level)
{
    seat_course_sound(level);
    seat_vs_countdown();
    /* the third statement of Stage::InitResources this file seats, and the one
       that puts the VS scoreboard and the match clock on screen; see its own
       block above for why it is the mask and not the HUD that was missing */
    seat_engine_a_layers();
}

/* The music half of the sound row, run after Stage::LoadClsnAndObjects (see
   seat_course_music). level_boot.cpp calls it at that point of the boot. */
void port_boot_course_music(int level)
{
    seat_course_music(level);
}

/* Stage::InitResources' prevLevel (:226), handed over by the port's latch
   (hal/level_change.cpp port_level_latch) at the moment it replaces
   gCurrentSublevel. seat_course_sound's same-area music guard reads it. */
void port_music_note_prev_level(int level)
{
    g_music_prev_level = level;
}



/* ---- A LONGER PRE-ROUND START COUNTDOWN ------------------------------------
 *
 * A host-side round mode asks for a full FIVE-SECOND wait before play starts, so
 * players can read the arena and the tagger cannot snowball off the spawn. This
 * does not invent a countdown: it drives the ROM's OWN VS start countdown -- the
 * one Stage::InitResources seats (data_0209f2bc = 3) and the matched
 * Stage::RenderVsModeCountdown draws as the READY? banner, the big red 3/2/1
 * numeral, then START. The ROM's countdown is ~2s; this stretches it to 5s off
 * the HOST FRAME COUNTER, so every peer runs the identical cadence and seeds the
 * tagger on the identical frame (shared state only -- no local clock, no rand). The
 * port hosts RenderVsModeCountdown NOWHERE for a normal VS round, and
 * the regular countdown hook steps aside when the mode is armed, so a normal
 * round is byte-unchanged.
 *
 * The schedule, in host ticks (walk_window runs the 3D logic at ~30Hz, so 30
 * ticks ~= one second), anchored at frame 90 -- the frame the tagger used to be
 * seeded, past the level-entry no-control freeze:
 *     [0,90)     count 3   held, silent (the level-entry freeze; see the drive)
 *     [90,150)   count 3   READY? + red 3    ~2.0s (the get-ready / spread beat)
 *     [150,195)  count 2   READY? + red 2    ~1.5s
 *     [195,240)  count 1   READY? + red 1    ~1.5s
 *      240       count 0   START  + port_luigi_seed picks the tagger (a full 5s)
 *     [240,270)  count 0   START held up (f304 non-zero so the glyph stays)
 * The native render pairs the READY? banner with the leading numeral, so the "3"
 * doubles as the get-ready beat held two seconds; 2 and 1 tick once a second.
 * data_0209f2bc != 0 across the window freezes the players exactly as the DS's
 * own 3-2-1 does (func_ov002_020c71e0), and it drops to 0 at the pick. */
extern void _ZN10dScStage_c21RenderVsModeCountdownEv(void);
extern void *gSceneFader;   /* the installed fader; the render derefs it, unguarded */

enum {
    kLiCdStart = 90,
    kLiCd3End  = 150,
    kLiCd2End  = 195,
    kLiCdPick  = 240,   /* count reaches 0: START + the tagger pick, a full 5s in */
    kLiCdEnd   = 270    /* START glyph held to here */
};




// =============================================================================
// THE OTHER END OF THE SAME STATEMENT: what happens when the VS clock runs out
// =============================================================================
//
// The VS countdown is the HEAD of Stage::Behavior's VS block. This
// is its TAIL, hosted here for the identical reason and in the identical shape:
// the whole function is not in the port's link, and this is one statement of
// it calling the ROM's own functions.
//
// The statement, src/_ZN5Stage8BehaviorEv.cpp:173-186, verbatim:
//
//     if (data_ov002_02111188 == 0 && data_0209f204 != 0) {
//         if (data_0209fc68 == 0) {
//             ... the single-console VE_Init/VE_Update arm ...
//         } else {
//             Scene::StartSceneFade(7, 0, 0);
//             data_02092778 = 1;
//             data_0209d4b0 = 0;
//         }
//         Sound::StopLoadedMusic_Layer1(0x3c);
//     }
//
// data_0209f204 is the match clock's own "time is up" flag and HUD::UpdateVsTimer
// is what raises it -- the port hosts the HUD as a sub-screen actor
// (hal/sub_actors.cpp) and its Behavior slot dispatches UpdateVsTimer every
// frame, so the clock the port runs is the ROM's clock. data_ov002_02111188 is
// the same function's sub-counter, which UpdateVsTimer reloads to 0x3c at the
// moment it raises the flag: the ROM therefore waits SIXTY FRAMES after time-up
// before it asks for the results, and the wait is part of the statement, not
// padding this could skip.
//
// data_0209fc68 is the wireless session state. It is nonzero in every VS run
// this port can produce -- VS is a wireless mode and there is no offline VS on
// the cartridge -- so the else arm is the live one and VE_Init is not reachable
// from here. Scene 7 is the VS results screen: the same dScEntry_c class as the
// scene 6 entry screen, told apart by its own id (src/_ZN10dScEntry_c13InitResourcesEv.cpp branches
// on self->unk_00c == 6 and hands everything else to _ZN10dScEntry_c19InitResultResourcesEv, the
// results screen with the winner calculation behind it).
//
// ---- WHAT THE PORT CAN AND CANNOT DO WITH THAT REQUEST ----------------------
//
// Scene::StartSceneFade is matched src and the port links it: it parks the
// pending scene id in gNextSceneId and writes the fade colour. What consumes a
// pending id is Scene::SpawnIfNecessary, and the port runs that ONLY on the
// scene path (hal/scene_boot.cpp's carrier inside port_scene_tick). On the LEVEL
// path -- which is where a VS arena runs -- there is no scene spawner, because
// crossing from a live level into a scene means tearing the Stage down first and
// port/stage_lifecycle_map.txt section 5 measures that teardown blocked on seven
// pieces. So the request is real, the ROM makes it, the port records it, and
// nothing consumes it. That is the honest state and this function reports it
// rather than papering over it.
//
// ---- AND IT IS WORSE THAN "NOTHING CONSUMES IT". MEASURED. ------------------
//
// Run R2 of this lane, both windows, from this tip with the request armed:
//
//     [vs] f2791 TIME UP: ... scores 3,0,0,0, stars taken 3
//       [vs] Scene::StartSceneFade(7, 0, 0): the VS RESULTS screen requested,
//            pending scene id now 7
//       [vs] f2792 pending scene id is now 7
//     [fx] f2820 ...
//     FATAL: Stage vtable slot 3 (CleanupResources) is not hosted
//
// Both processes, ~28 frames after the request, exit 127. Something DOES
// consume it, and what consumes it is the ROM's own Scene::BeforeBehavior,
// correctly seated in _ZTV5Stage slot 7 and dispatched on the Stage every
// frame. Its body -- quoted in full in hal/level_change.cpp's pending-scene
// block -- is: a pending id that is not the 0x187 sentinel means run the fader,
// and once the fader is AT THE END, ActorBase::MarkForDestruction(self). `self`
// is the Stage. So the ROM asks for the Stage to be torn down before the scene
// comes up, which is exactly right on the DS and is the one thing this port
// cannot do: port/stage_lifecycle_map.txt section 5 measures the teardown
// blocked on seven pieces, and slot 3 is the first of them.
//
// THAT IS THE SAME ABORT hal/level_change.cpp's port_scene_request_release was
// written to prevent -- port/exitlevel_seat.txt reproduces it, word for word
// and exit code for exit code, from a latched pending id on the level path.
// A ROM-faithful match-end request is simply another way to latch one.
//
// SO THE SCENE REQUEST IS OFF BY DEFAULT and the reason is not timidity. A
// default that turns "the match ends and both windows stand in the arena" into
// "the match ends and both windows abort" is a regression, whatever the ROM
// does. The switch stays because the next lane -- the one that hosts the Stage
// teardown and puts a scene carrier on the level path -- needs to be able to
// arm the request and watch where it stops, and this is that instrument.
//
// ---- AND THEN THE ROM MADE THE REQUEST ITSELF (run mp1, lane VSTIMEUP1) -----
//
// Everything above was written while Stage::Behavior was not in the link. It is
// now (_ZTV5Stage slot 6 holds the ROM's body, port/slice_gate220.txt), so the
// statement quoted at the top of this block runs on its own and asks for scene
// 7 every frame the guard is open, with this switch off. That turned every VS
// match that ran to the clock into the abort measured above, on every console,
// in the shipped kit and in a player's own match. The match-end watcher now
// answers a pending 7 in VS itself: this port's results screen is this end
// path (banner, MATCH OVER marker, the launcher room), so the sentinel goes back
// before Scene::BeforeBehavior reads the id and the Stage is never marked. The
// marker says results_screen=port when that happened. See the block at the top
// of the match-end watcher.
//
// ---- THE ENVIRONMENT --------------------------------------------------------
//
//   A switch turns the whole watcher off. Default ON: the time-up
//                           report and the ROM's own music stop are the ROM's
//                           behaviour at the ROM's own moment, and the marker
//                           costs a line in the flight recorder.
//   A second switch ALSO makes the ROM's Scene::StartSceneFade(7,0,0)
//                           request, and leave the ROM's own request standing
//                           (no answer). Default OFF. IT ABORTS THE PROCESS
//                           about thirty frames later, by the measurement
//                           above; it is the instrument that reproduces the
//                           abort from a fixed binary, not something to leave
//                           on.
//   A third quits the process once the end state is reached.
//                           Default OFF -- see the note over the exit.
//   A grace count sets the frames between the time-up statement and the
//                           marker. Default 240 (four seconds at 60fps), which
//                           is eight times the ROM's own 0x3c wait and long
//                           enough for a fade plus a scene bring-up to be
//                           visible if one ever happens.
//   A star target sets the SECOND win condition: first player to n
//                           stars ends the match through this same path.
//                           Unset or 0 is off and off is the pure cartridge
//                           behaviour. The launcher sets it, so the
//                           variable is the interface; see the trigger's own
//                           block below.
//
// OUTSIDE VS THIS IS ONE COMPARE, A DISARM AND A RETURN. gGameMode != 1
// leaves before anything else is read and before any getenv; the disarm clears
// the end latch so a second match in one process would arm again, and its own
// branch is taken at most once per match. Measured: an adventure selftest is
// byte-identical to the pre-change build at a matched .dsstate base.
extern "C" {
extern unsigned char data_0209f204;      /* VS "time is up"                   */
extern unsigned short data_ov002_02111188; /* UpdateVsTimer's sub-counter     */
extern int           data_0209fc68;      /* wireless session state            */
extern unsigned char data_02092778;      /* Stage::Behavior sets this with the
                                            results request                   */
extern int           data_0209d4b0;      /* and clears this                   */
extern unsigned short gNextSceneId;     /* Scene::SetSceneToSpawn's pending id,
                                            0x187 = nothing pending           */
extern signed char   gNumStageStars[];    /* the four players' VS star counts  */
signed char NumVsStarsObtained(void);
}
/* src/_ZN5Sound22StopLoadedMusic_Layer1Ej.cpp spells the C++ method out rather
   than an extern "C" face, so the MSVC-mangled name is what is in the link and
   no reverse bridge is needed -- hal/reverse_bridges.cpp's sound block names
   this function as its one exception for exactly that reason. Declaring the
   class here and calling the method is therefore how this TU reaches it. A
   member function's linkage is not affected by the enclosing extern "C". */
struct Sound { static void StopLoadedMusic_Layer1(unsigned int frames); };

/* Does the port ask for scene 7 by default? NO, and the measurement that says
   so is quoted in the block above: with it on, both windows abort on
   "Stage vtable slot 3 (CleanupResources) is not hosted" about thirty frames
   after the request. */
enum { PORT_VS_END_SCENE_DEFAULT = 0 };

/* The latch: the frame the end statement ran on, whether the marker has gone
   out, and which trigger fired. At file scope rather than inside the function
   so the mode test below can clear them, which is what re-arms this for a
   SECOND match in one process. Nothing produces one today -- going from a
   finished match back to the match setup is not something the port can do yet -- but
   a latch that silently swallows the second match is exactly the bug the lane
   that adds the return to setup would spend a day chasing.

   ---- AND THE RE-ARM IS A TRAP UNTIL SOMEBODY CLEARS THE SCORES -------------

   READ THIS BEFORE MAKING AN IN-PROCESS SECOND MATCH WORK. This latch re-arms,
   but gNumStageStars -- the ROM's per-player carried-star array, the thing every
   trigger and both markers read -- IS NEVER CLEARED IN THIS PORT. A second
   match in the same process would therefore start with the first match's
   scores: the star target would fire on frame one against stale values, and the
   marker would report a winner nobody played for.

   IT IS DORMANT TODAY AND THAT IS THE ONLY REASON IT IS A NOTE. A launcher
   rematch spawns a FRESH PROCESS (the
   launcher waits for exit, then the room re-arms and the next match is a new
   launch), so nothing in the shipped flow reaches a second match in one
   process. The day something does, this is where it breaks.

   THE ROM DOES CLEAR IT, and the loop is already written -- in
   src/_ZN5Stage13InitResourcesEv.cpp:181-197, which this port does not run
   (port/stage_lifecycle_map.txt section 2d lists gNumStageStars among the
   per-player clears with seat "none"):

       if ((s32)gNumPlayers > 0) {
           ... for idx in 0 .. gNumPlayers-1:
                   if (sl || (v1 == 0 && f26c != 1)) gNumCoins[idx] = 0;
                   gNumRedCoins[idx] = 0;
                   gNumStageStars[idx] = 0;
       }

   NOT SEATED HERE, deliberately. It sits inside a guard three conditions deep
   (`b1 || sReturnState == 2 || v1 || temp_r4 == 0x1D || temp_r4 != temp_r0`,
   line 169) over state this file does not carry, and it clears two more arrays
   besides -- the coin counts and gNumRedCoins. Hosting a partial copy of a
   guarded ROM block on a path nobody can exercise is how a wrong clear gets
   shipped and stays invisible; the honest move is to name the block, the file
   and the line so the lane that needs it seats the whole thing with its guard.
   That lane also owns deciding whether the port's f21c is the count the ROM
   means here. */
static int g_end_fired = -1;
static int g_end_scores[4];






static int g_king_points[4];





/* The live behaviour list, walked to count the stars IN PLAY for the king probe:
   node[1] is next, node[2] is the actor, its type id is at +0xc (0xb2 STAR /
   0xb3 SILVER_STAR) and its state at +0x440. A VS arena always carries the four
   table stars parked DORMANT in state 9 (the caged markers) -- those are not
   "a star in play", so they are excluded. What is counted is the awake star:
   collectable, held, tossed or dropped -- anything but state 9. The king
   one-star invariant is that this is never more than 1. */
extern int data_020a4b78[8];




/* ---- THE FREEZE ------------------------------------------------------------
 *
 * Tango: first-to-N must stop play AT the target. Two halves, and only the
 * first is load-bearing for the marker:
 *
 *   THE SCORES ARE LATCHED at the trigger (above), so the marker reports the
 *   moment the match was decided whatever happens afterwards. That alone kills
 *   the 3,0,0,0 quirk and it cannot fail.
 *
 *   THE PADS ARE HELD from the frame after, so nothing can be played. All four
 *   slots, every button and stick word the ROM's readers use -- the same seven
 *   fields walk_window's own comms fan-out writes. With no input no player can
 *   swing at a ball, so no star can be released and none collected: "no
 *   scoring" falls out of "no play" rather than being a second mechanism.
 *
 * WHY NOT SKIP THE TICK, which is the debug menu's freeze and the obvious move.
 * The menu's `game_ticked = 0` skips the whole else-branch of the level loop,
 * and the comms exchange lives inside it. Two windows that stop exchanging --
 * even in perfect step -- are two windows whose transport is no longer being
 * pumped, and this lane is not going to find out what that does to a live
 * session at the one moment the match is trying to end cleanly. Holding the
 * pads leaves every frame doing exactly what it did, minus the input.
 *
 * SAFE ACROSS CONSOLES BY CONSTRUCTION: the hold is derived from the end latch, the
 * end latch is set by a test on state both consoles share, and both measured
 * setting it on the same frame. Neither window is told anything by the other.
 *
 * Its off switch leaves the pads live and keeps only the latch. */
extern "C" {
extern char data_0209f49c[];   /* held        */
extern char data_0209f49e[];   /* pressed     */
extern char data_0209f4a0[];   /* stick / etc */
extern char data_0209f4a2[];
extern char data_0209f4a4[];
extern char data_0209f4a6[];
extern unsigned char data_0209f4ac[];  /* touching */
}
static char g_vs_names[4][17];










/* THE LEAVER'S BODY GOES, THE WAY THE CARTRIDGE TAKES A PLAYER OUT (plan G1).
   The DS drops a lost child mid-match in src/func_02030b58.c, the in-match
   wireless state's per-frame check: for each seat whose radio link is gone it
   sets that seat's bit in data_0209fc58 and clears data_0209fc5c[i], the "this
   seat is live" byte. From then on Player::Behavior returns at its first test
   for that seat in VS (src/_ZN6Player8BehaviorEv.cpp:73), daPly_c::Render draws
   nothing (src/_ZN6Player6RenderEv.cpp:46), and anything else that reads the
   same byte is gone too. The port does not run that
   function (its closure is the WM stack; hal/arm9_tables_link100.cpp says
   why), so the same two writes are made here, from the seat mask, at the same
   round on every console. The ROM's loop covers its four seats, and
   data_0209fc58's upper bits mean other things (src/func_02030e4c.c), so seats
   4..15 clear only their live byte.
   PARKED as well, which the cartridge does not do. Its body would stay where
   it stood, invisible but still the nearest player for an enemy's
   dActor_c::ClosestPlayer, and still an icon on the bottom-screen map
   (Minimap::Behavior places every seated body whatever its live byte says).
   Moved far out of the level and stopped (no Behavior runs to move it back:
   no fall, no pit), it is nobody's nearest player and off the map. The spot is
   the body's own position plus fixed offsets -- shared state only, so the
   same spot on every console -- at a height already proven far enough out
   against ClosestPlayer. */
extern "C" unsigned char data_0209fc5c[];   /* per-seat live byte, 16 wide */
extern "C" unsigned char data_0209fc58;     /* the ROM's "seats lost" bits  */
enum : int {
    kVsParkUp   = 200000 << 12,              /* Fix12 world units */
    kVsParkSide = 100000 << 12,
};


/* A LEAVER'S STARS SPILL WHERE IT STOOD (plan G1, Tango's call: spill them if
   the game's own drop does it cleanly). The drop is the ROM's own, the one a
   hit runs (daPly_c::St_Hurt_Main -> _ZN7daPly_c19func_ov002_020d94ccEv): one carried star off
   gNumStageStars and one VS star actor (0xb2) spawned at the body, flying the
   way the body faces. It is called once per carried star, turning the body a
   fraction of a circle between calls so the stars fan out instead of landing
   in one pile; the body is parked right after, so the turn is never seen. A
   star the ROM could not spawn (actor table full) is already off the count --
   the drop decrements first -- and so leaves with the player. The same calls
   at the same round on every console: the stars are shared state like any
   other drop. A round mode's single star goes back on the field this way. */
extern "C" int _ZN7daPly_c19func_ov002_020d94ccEv(char *self);
extern "C" char *_ZN8dActor_c15FindWithActorIDEjPS_(unsigned id, char *prev);
extern "C" char *_ZN8dActor_c10FindWithIDEj(unsigned id);



extern "C" signed char data_ov002_02111148;   /* the map area being shown */
extern "C" void *data_0209b468;               /* the live-actor list head */







extern "C++" {   /* this part of the file sits inside an extern "C" block: these are C++ names */

namespace port {
}
}












/* Seat what the course loop reads that the boot does NOT: the player globals
   (SetPlayerGlobals, which InitResources does not call -- the handoff and the
   title path do). The sound row moved into the boot (port_boot_course_sound),
   so this no longer seats it. Called once from the harness after the boot.

   ONCE PER SESSION IS CORRECT, AND THE ONE-SHOT IS NOT A RESPAWN BUG. Read the
   relocs before "fixing" this: SetPlayerGlobals (0x0202acfc) has three callers
   in the ROM -- StartFile (0x0202ae88), _Z13PrepareVsModev and the ov003
   title-confirm path _ZN10dScTitle_c8BehaviorEv -- plus two the port adds, this one
   and hal/level_change.cpp:1415 (the port's copy of that ov003 path). It is
   the NEW FILE seat: lives to 4, health to 0x880 for all four players. This
   call stands in for StartFile's, so it belongs exactly where it is.

   Two of the ROM's three DO enter a level -- StartFile and _ZN10dScTitle_c8BehaviorEv
   both call LoadLevelNoReturn immediately before it, so do not repeat the
   earlier claim here that "none is a level entry". The distinction that
   matters is narrower and is the whole reason this stays a one-shot: none of
   them is on the PER-ENTRY OR RESPAWN path. They run when a file is started,
   never when a running file re-enters a level.

   Re-running it per entry would be WORSE than the bug it looks like it causes:
   it would restore lives to 4 on every respawn, so _Z13HitDeathPlanei's
   `gNumLives[0] != 0` game-over branch below could never be taken.

   The ROM's PER-ENTRY health restore is a different mechanism entirely and does
   not pass through here: the Player's own level-enter step heals to full when
   the latched entry reason says fresh-or-death (src/game/player/Player.cpp:29),
   and the boot latches that reason -- see the gPrevReturnState seat in
   hal/level_boot.cpp. TRIAGE14 read this one-shot as the cause of the infinite
   death loop; the cause was the missing latch, and this note is here so the
   next reader does not walk the same path. */
void port_course_seat(void)
{
    if (g_seated) return;
    g_seated = 1;
    seat_player_globals();
}

// =============================================================================
// COINS -- and the seam with the coin actor
// =============================================================================
//
// THE SEAM IS Actor::GivePlayerCoins, which is Nintendo's own and is now in
// the link (slice_gate35.txt). It is the single point where a coin actor
// stops being a coin actor and becomes a number:
//
//     void Actor::GivePlayerCoins(daPly_c &p, u8 count, u32 kind)
//       -> Sound::PlayBank3(0x11 or 0x12, this->pos)     the chime
//       -> _Z9GiveCoinsii(p.mPlayerNo, count * data_02075238[kind])
//       -> daPly_c::Heal(&p, amount << 8)                 coins heal
//       -> Actor::Spawn(data_02075230[kind], ...)        the count popup
//
// A coin class calls it from its own Behavior on the frame its collider
// reports the player, with `kind` 0 for yellow, 1 for red and 2 for blue.
// Nothing else is needed on the coin side and nothing here reaches into the
// coin: the ROM function is the whole contract.
//
// port_give_player_coins is the same call with a C name, for callers that
// have not got an Actor in hand (the probe below, and any host-side collect).

void port_give_player_coins(void *actor, void *player, int count, int kind)
{
    _ZN8dActor_c15GivePlayerCoinsER7daPly_chj(actor, player,
                                          (unsigned char)count,
                                          (unsigned int)kind);
}

// =============================================================================
// THE STAR
// =============================================================================
//
// port_star_collect is the level-independent half of func_ov002_020e8ef0
// (PowerStar's own collect handler) followed by func_ov002_020c94a4's tail.
// Both are matched src; neither can be CALLED here, because the first wants a
// live PowerStar actor and the second a Player already in the star-get state,
// and the PowerStar class is not registered yet -- its InitResources is 0x820
// bytes of star-model, silver-star and red-coin-star loading, and the castle
// grounds names no star to hang it on.
//
// So this is the bookkeeping and the handoff, written out, and it is the same
// sequence in the same order:
//
//   Event 0x1e   "a star has been got this session". func_ov002_020e930c
//                returns early on it, which is what stops a second star from
//                starting a second cutscene.
//   Event 0x1d   the HUD's own bit: HUD::UpdateHealthMeter forces the health
//                meter into state 5 (retract) while it is set.
//   gCollectedStarId / gNewStarCollected   which star, and whether it was NEW -- the
//                two words the star-select screen reads back on the way out.
//   _Z21CollectStarInCurLeveli           the save bit.
//   func_02012790(0x2d)             the fanfare, Sound::Play2D(2, 0x2d).
//   _Z9ExitLevelv()                     _Z12SetNextLeveli(1) + _Z22StartExitCharacterWipev,
//                                   which is the course-clear handoff itself:
//                                   next level 1 (the hub), and the wipe the
//                                   current character's row of data_020755bc
//                                   selects.
//
// What it is NOT: the star's spin, Mario's hands-up animation, and the
// "COURSE CLEAR" message. Those are the cutscene, they live in the PowerStar
// actor's state table and in Message::, and they are honestly missing rather
// than faked. The handoff after them is here and fires.

int port_star_collect(int starId)
{
    if (_ZN5Event6GetBitEj(0x1e)) {
        fprintf(stderr, "[star] already collected this session -- ignored\n");
        return 0;
    }
    const int wasNew = _Z25IsStarCollectedInCurLeveli(starId) ? 0 : 1;

    _ZN5Event6SetBitEj(0x1e);
    _ZN5Event6SetBitEj(0x1d);
    gCollectedStarId = (unsigned char)starId;
    gNewStarCollected = (unsigned char)wasNew;
    _Z21CollectStarInCurLeveli(starId);
    func_02012790(0x2d);

    fprintf(stderr, "[star] collected star %d in sublevel %d (%s), "
            "total now %d\n", starId, (int)gCurrentSublevel,
            wasNew ? "NEW" : "already had it", (int)_Z8NumStarsv());

    _Z9ExitLevelv();
    fprintf(stderr, "[star] course clear: _Z9ExitLevelv -> next sublevel %d "
            "entrance %d, reason %d, wipe for character %d\n",
            (int)gNextSublevel, (int)sNextEntranceId, (int)sReturnState,
            (int)data_0209caa0[0x41]);
    return 1;
}

// =============================================================================
// The probe surface
// =============================================================================
//
// One entry point per thing the gate has to be able to SHOW moving. The
// harness drives them from a frame number; nothing here runs on its own.

int port_course_health(void)
{
    return gPlayerHealth[gLocalPlayerId] >> 8;
}

/* Sound, measured rather than asserted. Voices and sequencer players are the
   two things that either exist or do not: sd_mix_active is the mixer's own
   "is this hardware channel sounding", and sd_seq_active is "is this player
   still walking its sequence". A submitted command that allocated nothing
   shows up here as zeroes, which is the failure the [snd] log alone cannot
   tell apart from silence in the source material. */
extern int func_02049018(const int *camSpacePos);
extern int data_02099fac;   /* the 3D distance limit, romdata */
extern int data_02099fb0;   /* the type-9 voice-pool count, romdata */

int port_course_sound_probe(const char *when)
{
    int voices = 0, players = 0;
    for (int i = 0; i < 16; ++i) if (sd_mix_active(i)) ++voices;
    for (int i = 0; i < 32; ++i) if (sd_seq_active(i)) ++players;
    fprintf(stderr, "[snd-probe] %s: %d/16 voices sounding, %d/32 sequence "
            "players running, bank=0x%02x group=%d music=%d\n",
            when, voices, players, (unsigned char)sCurrentBank,
            (int)sLoadedGroup, g_course_music);
    /* the two numbers that decide whether a POSITIONAL sound is allowed to
       exist at all, plus where the listener thinks the player is */
    const char *p = (const char *)gPlayers[gLocalPlayerId];
    if (p) {
        const int *cs = (const int *)(p + 0x74);
        fprintf(stderr, "[snd-probe]   listener-relative player = "
                "(%d, %d, %d) scene, distance %d, limit %d, "
                "positional voices %d\n",
                cs[0] >> 12, cs[1] >> 12, cs[2] >> 12,
                func_02049018(cs), data_02099fac, data_02099fb0);
    }
    return voices;
}

/* ---- the looping-sound reap, observed rather than asserted ----------------
 *
 * The per-frame reaper (_Z13func_020119c8Pv) and the level-change reaper (_Z13func_02011974Pv)
 * both walk data_0209b53c, PlayLong's 0x40-entry handle table. An entry is LIVE
 * exactly when its first word -- the handle its owner is holding -- is non-zero.
 * This is the same count sd_vtrace_loop_census prints, exposed as a number the
 * harness can read without the voice trace on. */
int port_course_loop_live(void)
{
    int live = 0;
    for (int i = 0; i < 0x40; i++)
        if (data_0209b53c[2 + i * 5])   /* +8 header, then 0x14 (5 words) each */
            live++;
    return live;
}


int port_course_coins(void)
{
    return _Z8NumCoinsv();
}

/* The handoff, OBSERVED rather than caused.
 *
 * _Z12SetNextLeveli is the only thing that writes gNextSublevel / sNextEntranceId /
 * sReturnState, and it has exactly three callers: _Z9ExitLevelv (course
 * cleared), KillPlayer (died) and _Z13HitDeathPlanei (fell out of the world). So a
 * change in that triple means the course has ended and nothing on the port
 * had to say so.
 *
 * A CHANGE, not a non-zero value. gNextSublevel is a NAMED romdata entry, so
 * it boots with the ROM's own byte in it (0xff), and "is it non-zero" reads
 * as fired before anything has happened -- which is what the first version of
 * this probe reported. */
int g_arm_watch[3];

void port_course_arm_watch(void)
{
    g_arm_watch[0] = gNextSublevel;
    g_arm_watch[1] = sNextEntranceId;
    g_arm_watch[2] = sReturnState;
}

int port_course_handoff_fired(void)
{
    return gNextSublevel != (unsigned char)g_arm_watch[0] ||
           sNextEntranceId != (unsigned char)g_arm_watch[1] ||
           sReturnState != (unsigned char)g_arm_watch[2];
}

int port_course_next_sublevel(void)
{
    return (int)gNextSublevel;
}

/* Is he in a state that ends the life? data_ov002_0211010c is the one
   daPly_c::Hurt changes to when _ZN7daPly_c19func_ov002_020d91e0Eii answers "dead", and
   data_ov002_02110124 is its water twin -- the two the ROM itself compares
   against in daPly_c::Heal to refuse healing a corpse. Comparing the state
   POINTER is exact; comparing a printed address is not. */
extern int data_ov002_0211010c[];
extern int data_ov002_02110124[];

int port_course_in_dead_state(void *player)
{
    const void *st = *(void **)((char *)player + 0x370);
    if (st == (void *)data_ov002_0211010c) return 1;
    if (st == (void *)data_ov002_02110124) return 2;
    return 0;
}

/* The HUD's own displayed health. St_DeadHit_Main will not call KillPlayer
   until this has ticked down to 0, which is why the death is not instant: the
   meter empties one wedge at a time and the state machine waits for it. */
int port_course_hud_health(void)
{
    extern unsigned char gMeterShownHealth;
    return (int)gMeterShownHealth;
}

/* Damage, through the game's own daPly_c::Hurt.
 *
 * `kind` is the ROM's damage class: 1 is a normal hit (one wedge), 2 and up
 * are the heavy ones that also knock Mario onto his back. Hurt derives the
 * knockback angle from `src`, so the probe passes a point a body-length in
 * front of him -- that is what an enemy standing there would be.
 *
 * Hurt itself does not subtract the health. It calls _ZN7daPly_c19func_ov002_020d91e0Eii,
 * which is the piece that reads the cap state, applies the 1.5x no-cap
 * penalty, calls daPly_c::Heal(-damage) and answers "is he dead now" -- and
 * on 1 it changes the state to the DEAD one rather than the hurt one. So the
 * whole death decision is the ROM's; this only supplies the hit. */
/* The ROM's own "may he be hurt right now" gate: the invincibility timer, the
   mega/no-control flags and the already-taking-damage flag. An enemy's
   collider hits every frame it overlaps and daPly_c::Hurt refuses on all but
   the first, so a probe that wants to land a SECOND hit has to ask this the
   same way rather than count frames. */
int port_course_can_hurt(void *player)
{
    return _ZN7daPly_c19func_ov002_020d82f0Ev(player);
}

int port_course_hurt(void *player, int kind)
{
    char *p = (char *)player;
    int src[3];
    const int hp_before = port_course_health();
    src[0] = *(int *)(p + 0x5c) + 0x8000;   /* 8 units in +X */
    src[1] = *(int *)(p + 0x60);
    src[2] = *(int *)(p + 0x64);
    const int r = _ZN7daPly_c4HurtERK7Vector3jihhh(
        player, src, (unsigned)kind, 0x8000, 0, 0, 1);
    const int dead = port_course_in_dead_state(player);
    fprintf(stderr, "[hurt] kind=%d -> Hurt returned %d, hp %d -> %d, "
            "step=%u, state=%s\n", kind, r, hp_before, port_course_health(),
            *(unsigned char *)(p + 0x6e3),
            dead == 1 ? "DEAD (data_ov002_0211010c)"
                      : dead == 2 ? "DEAD-WATER (data_ov002_02110124)"
                                  : "alive");
    return r;
}

/* The drowning tick. daPly_c::St_HurtWater is the underwater damage state and
   the breath meter is the same health word, so drowning is Heal(-n) applied
   while mIsUnderwater is set -- which is what the ROM's own water damage
   does. Spelled here so a probe can drive it without a water volume. */
int port_course_drown_tick(void *player, int amount)
{
    _ZN7daPly_c4HealEi(player, -amount);
    return port_course_health();
}

/* The death handoff, as the ROM fires it. St_DeadHit_Main calls KillPlayer
   once the HUD's displayed health has ticked to zero; this is the same call
   for a harness that wants it now. */
void port_course_kill(void)
{
    const int lives = gNumLives[0];
    KillPlayer();
    /* The handoff, read back as data: _Z12SetNextLeveli fills the next sublevel
       and its entrance out of data_02075638, and StartExitFaderWipe(4) arms
       the wipe. Printing the two words is the difference between "the call
       returned" and "the call did something". */
    fprintf(stderr, "[death] KillPlayer: lives %d -> next sublevel %d "
            "entrance %d, reason %d, exit wipe 4 armed\n",
            lives, (int)gNextSublevel, (int)sNextEntranceId,
            (int)sReturnState);
}

/* Respawn: put the health back the way the game does between lives and hand
   the player to the respawn state. gPlayerHealth is what SetPlayerGlobals
   fills at 0x880 and what every life starts from. */
void port_course_respawn(void *player)
{
    gPlayerHealth[gLocalPlayerId] = 0x880;
    _ZN5Event8ClearBitEj(0x1d);
    fprintf(stderr, "[respawn] health restored to %d, health meter released\n",
            port_course_health());
    (void)player;
}

}  // extern "C"

// =============================================================================
// _ZN11KuppaScript18StartDanceCutsceneEi: the star-get camera-script launcher -- a u8-return ride-through
// =============================================================================
//
// src/func_0200ee8c.c is the star-get cutscene's script selector:
//
//     void _ZN11KuppaScript18StartDanceCutsceneEi(s32 arg0) {
//         if (arg0 < 0) arg0 = _Z20GetStarCameraSettingi(data_0209f224);
//         _ZN11KuppaScript3RunEPh(sDanceCutsceneTable[arg0]);   // 6-entry table
//     }
//
// It declares `extern s32 _Z20GetStarCameraSettingi(s32)` and uses the FULL return as
// the index into sDanceCutsceneTable[6]. But _Z20GetStarCameraSettingi is defined
// `unsigned char _Z20GetStarCameraSettingi(int)` -- `(data_02092134 >> (idx*4)) & 0xf`
// -- so it only ever sets AL (verified in the obj: `and al,0Fh; ret`). On ARM the
// s8/s32 declaration mismatch is harmless: the ROM's `bl` leaves the byte in r0
// and the AND already cleared the top bits, so `sDanceCutsceneTable[r0]` indexes 0..5.
//
// On x86 MSVC the __cdecl caller reads the WHOLE of EAX, whose high bytes
// _Z20GetStarCameraSettingi never wrote, so a call that should return 1 came back as
// 0x00001001 = 4097. `sDanceCutsceneTable[4097]` reads 0x4000 bytes past a 24-byte
// table, hands _ZN11KuppaScript3RunEPh a garbage pointer (0ffaff1e), and _ZN11KuppaScript7ProcessEv
// faults c0000005 the first frame of the star-get cutscene. MEASURED on the
// natural king-defeat + touch path (SM64DS_LEVEL=6 SM64DS_KING_FORCE_DEFEAT=90,
// star state 4 -> collect handler -> St_NoControl_Main -> func_ov002_020c7ff8 ->
// _ZN9dCamera_c18StartDanceCutsceneEji -> here).
//
// The fix is the sound_abi.cpp / _Z13HitDeathPlanei pattern: host the one function and
// spell the return the width the ROM means. _Z20GetStarCameraSettingi yields a 4-bit
// value, so masking its result with 0xf reproduces exactly the ARM behaviour (the
// only bits the ROM's own `& 0xf` and single-byte return could carry) and keeps
// the index inside the 6-entry table. src/func_0200ee8c.c is filtered out of
// slice_gate10.txt in exchange.
// PORT_HOST_ABI: _Z20GetStarCameraSettingi returns u8 (AL only); the src reads it as
// s32 and indexes with it, so mask to the ROM's own 0xf here.
extern "C" {

/* PORT_HOST_ABI: _Z20GetStarCameraSettingi returns u8 (AL only); cdecl reads all
   of EAX, so mask to the ROM's own 0xf here (see the header above). */
/* _ZN11KuppaScript18StartDanceCutsceneEi RETIRED (run link100, lane SEAT6, batch B6).
   The u8-return ride-through is a DECLARATION, not a body: the callee
   src/GetStarCameraSetting.c returns `unsigned char` and ends
   `and r0,r0,#0xf / and r0,r0,#0xff`, so masking here said the same
   arithmetic a second time.
   The matched TU src/func_0200ee8c.c is seated in its place: port/tools/hostgen.py's ARG_WIDTH table declares the return the width
   the definition really has, which makes MSVC read AL alone.
   Per-row ROM evidence (referrer, RTTI name, kind:function record, the
   dispatch instruction read at its own address) is in port/slice_seat6.txt. */
}  // extern "C"
