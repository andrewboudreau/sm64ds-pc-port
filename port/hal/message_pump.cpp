// The message box's HOST SIDE, around the ROM's own Stage::UpdateMessage.
//
// WHAT RUNS NOW (run linkfull, lane RS5B). Stage::UpdateMessage is the ROM's
// own body: src/_ZN5Stage13UpdateMessageEv.cpp, main's blob (c45176b308 via
// #3107), which declares the four callees it used to stub -- Message::
// UpdateWindow, Message::Update, Message::DisplaySaving, SaveData::
// SaveCurrentFile -- and calls the real ones. Stage::Behavior (_ZTV5Stage slot
// 6) calls it once a frame when gGameMode != 1, exactly as on the DS:
//
//     void Stage::UpdateMessage() {
//         if (data_0209d660 == 0) return;              // no message active
//         if (Message::UpdateWindow()) {               // box fully open?
//             if (data_0209d654 == 0) { Message::Update(); return; }  // dialogue
//             ... the save-screen countdown ...        // data_0209d654 != 0
//         }
//         ... the save-screen close ...
//     }
//
// This file used to BE that body, statement for statement (port_message_pump),
// because the matched file defined empty stubs of its four callees. Main fixed
// the file; the host transcription of the save arm, the SaveCurrentFile leaf
// and the pump's own per-frame duties retire with it. What is left here is
// what the ROM body needs from the host, and nothing else:
//
// 1. THE FORWARD. hal/stage_frame.cpp's face `Stage::UpdateMessage()
//    { port_message_pump(); }` still defines ?UpdateMessage@Stage@@SAXXZ, the
//    name Stage::Behavior spells (that file belongs to another lane this wave).
//    So the ROM body is compiled under a per-source rename, UpdateMessage ->
//    UpdateMessage_rom (port/CMakeLists.txt, the RS5B block), and
//    port_message_pump is one call into it. The day stage_frame.cpp's face is
//    deleted, the define goes, the ROM body takes its own name back, and this
//    function goes with them.
//
// 2. ?Update@Message@@SAXXZ. The ROM TU declares Message::Update STATIC (it
//    reads only globals, and Stage::UpdateMessage calls it with nothing in r0);
//    the matched body, src/_ZN7Message6UpdateEv.cpp, is a member
//    (?Update@Message@@QAEXXZ), faced for C callers in hal/reverse_bridges.cpp
//    as _ZN7Message6UpdateEv(void *). The static spelling is defined below and
//    forwards to that body with the same null receiver the pump always passed.
//
// 3. THE ENGINE-A DISPLAY SYNC, the one host duty the box needs, riding that
//    face. On the DS _Z13func_02019144v copies the software BG-offset shadows and
//    the BG-enable layer mask into the engine-A registers once a frame.
//    SetBg3Offset (src/SetBg3Offset.c) writes shadows, not hardware, and
//    _Z13func_0201f32ci scrolls BG3 through them so the box content (at the TOP of
//    the BG3 tilemap) lines up under the WIN0 window lower down the screen.
//    Without the flush the hardware BG3 offset stays 0 and the compositor reads
//    only the empty fill tile inside the window (the 2026-08-08 "box active but
//    0 px composited" finding). hal/sub_screen.cpp reproduces _Z13func_02019144v's
//    other beats; this is the engine-A remainder, byte for byte as _Z13func_02019144v
//    lines 46 and 55 write it, at the point the pump always ran it: right after
//    Message::Update in the dialogue arm, so it touches nothing when no box is
//    open.
//
// 4. TWO DATA SPELLINGS. The ROM TU declares its six globals at file scope in
//    C++, so it asks for their decorated names. Four were already bridged
//    (hal/cxx_aliases.cpp, romdata.c); data_0209d654 and data_0209d67c are
//    bridged below, onto the same hal/auto_bss.cpp storage every other reader
//    uses.
//
// WHAT RETIRED, and why each is safe:
//  - The pad publish (A/B from the Ctrl block OR-ed into PadData[i].pressed and
//    data_020a0e5a while a box was up). It existed because nothing wrote
//    data_020a0e5a, so IsButtonInputValid and Message::Update never saw a press
//    and the box could not be dismissed. The game loop's own input steps write
//    it now (run linkfull, lane B5INPUT: phase 0x17, func_0203bc7c builds
//    PadData and data_020a0e5a is mirrored from PadData[i].pressed right after
//    it, tests/walk_window.cpp), and the two Ctrl bits the publish carried
//    (0x1 punch, 0x2 jump) are raw A and raw B by construction
//    (host_btn_to_raw_keys), so the OR added nothing: measured, the word at
//    every Message::UpdateWindow call is identical with and without it.
//  - SM64DS_SAVE_POINT (lane SAVE1's instrument that fired DisplaySaving(0x295)
//    on a chosen pump frame) and SM64DS_MSG_PUMP_DEBUG (the per-frame box
//    trace). Nothing under port/tools reads either; both were properties of
//    the host pump, not of the game.

#include <cstdio>
#include "sdat/sdat.h"   /* sd_seq_player_mask / sd_seq_active: is the tune still sounding */

/* The box globals are src/Message.cpp's own, defined there with C++ linkage
   (?data_0209d660@@3EA and friends), so they are declared the same way here and
   reach that storage, not hal/auto_bss.cpp's C-named bytes (catchup1 round 3). */
extern unsigned char data_0209d660;   /* a message is active */
extern unsigned char data_0209d654;   /* 0 = the dialogue arm, else the save screen */
extern unsigned char data_0209d6bc;   /* the box's state (3 = the lines scrolling) */
extern short data_0209d6d4;           /* the open box's message id */

extern "C" {


extern unsigned char gMainScreenLayers;   /* engine-A BG-enable layer mask */
extern short gBg3HOffset, gBg3VOffset;   /* BG3 hofs/vofs shadows */

}

/* RETIRED (catchup1 round 3, alternatename_guard DEFEATED): src/Message.cpp
   defines ?data_0209d654@@3EA and ?data_0209d67c@@3EA itself, so the two aliases
   onto hal/auto_bss.cpp's C names never fired. */

static void port_message_flush_engine_a_regs(void)
{
    typedef volatile unsigned int vu32;
    /* engine-A DISPCNT layer-mask publish (_Z13func_02019144v line 46): puts BG3 on
       (gMainScreenLayers bit 3, which _Z13func_0201f32ci set), so the compositor's
       BG-enable read sees the box layer. */
    *(vu32 *)0x4000000 = (*(vu32 *)0x4000000 & ~0x1f00u) | (gMainScreenLayers << 8);
    /* BG3 offset (_Z13func_02019144v line 55): the box's own scroll, so the content
       at the top of the BG3 tilemap lines up under the WIN0 window. This is the
       one offset the box actually uses; BG0/1/2's offset shadows are not seated
       on the port and the box does not scroll them, so only BG3 is flushed. */
    *(vu32 *)0x400001c = (gBg3HOffset & 0x1ff) | (0x1ff0000u & (gBg3VOffset << 16));
}

/* The ROM TU's spellings. Message is declared here the way the ROM TU declares
   it, not from include/Message.h, because the matched Update is a member there
   and the static face is exactly what this file must not confuse with it. */
struct dScStage_c {
    static void UpdateMessage_rom();   /* src/_ZN5Stage13UpdateMessageEv.cpp, renamed */
};

/* ?Update@Message@@SAXXZ: the ROM body's dialogue-arm call. The matched
   Message::Update, then the engine-A sync the box needs on the port. */
/* TEXT2D1: 1 when this tick's Message::Update ran the box's line scroll (state 3: _Z13func_0201adfcv moves the text tile rows up, 4 DS pixels
   a tick). The compositor takes it for this tick's picture (port_text_scroll_take); a picture between two ticks then slides the text
   back down by what is left of the step. */
static int g_text_scroll_tick;
extern "C" int port_text_scroll_take(void) { int v = g_text_scroll_tick; g_text_scroll_tick = 0; return v; }
/* catchup1 round 2: src/Message2.cpp defines ?Update@Message@@SAXXZ itself
   now, so the static face that used to stand here was a second definition,
   and its inner call (_ZN7Message6UpdateEv in hal/reverse_bridges.cpp, which
   calls Message::Update()) would have come straight back to it. The host's
   two duties move to the call site instead: port_message_pump below runs them
   right after the ROM's Stage::UpdateMessage, on the same dialogue arm the
   ROM body takes (a message active, data_0209d654 == 0). Nothing here calls
   Message::Update, so there is no path back into this file. */

/* PEACHJINGLE1 / PEACHJINGLE2: the cartridge's NCS_BGM_PEACH (SDAT sequence 70, about 5 s) when a star QUOTA message opens, the box that comes
   up by itself once the star count reaches a door's number ("You've recovered 3 Power Stars! Now you can open any door with a 3 on it.").
   The DS plays nothing there. _ZN7daPly_c19func_ov002_020c6e14Ev (src/game/player/Player.cpp) opens it: when the last star was a new one (gNewStarCollected) and
   _Z8NumStarsv() is 1, 3, 8, 12, 30, 50 or 80 it shows data_ov002_020ff1a0[n] = message 0x178..0x17e. It runs from daPly_c::func_ov002_020c71e0 when
   the player lands back in the castle after a star exit, and from daStar_c's save-prompt state (step 4) after a star that does not end the
   course. The door's own "Reacting to the Star Power" (0x188) and the gate's "The Power Stars have broken the lock" (0x29) play nothing, as on
   the DS. The box opening (_Z13func_0201f32ci) sets data_0209d660 and the id data_0209d6d4, so this watches that edge once a frame.
   HOW IT PLAYS. The cartridge files sequence 70 under the MAIN music player (like the two Lakitu tunes), so it takes the level music's place rather than
   mixing over it. The game's own way to do that and give the level music back is the temporary-music pair the cap power-ups use (Player.cpp, the
   invincibility / metal / wing tunes): Sound::SetMusic(player, id) swaps the id in, Sound::EndMusic(player, ..) puts the level's own id back and restarts
   it. Both ignore any player but the local one (gLocalPlayerId), so nothing here reaches another player. It is not started while a boss layer (e444) or
   another temporary tune is up, and it is only ended if sequence 70 is still the current music (a level change or the game's own end of a tune that
   replaced it leaves nothing for us to undo). While it plays the volume target is held at full so a duck under the box does not make the tune a
   whisper; when it ends the target the game wants is handed back. Audio only. Setting PeachJingle (default on); off never calls into the sound code,
   so the audio is the DS's. */
extern "C" {
extern int gCurrentMusicID;                /* the current music id (layer 1 / temporary tune) */
extern int sLayer1Music;                /* the level's own music id */
extern int sLayer3Music;                /* the boss layer's id, -1 = none */
extern signed char sMusicVolumeTarget;        /* the music volume it is being faded to */
extern unsigned char gLocalPlayerId;      /* the local player's number */
void _ZN5Sound8SetMusicEjj(unsigned int player, unsigned int id);
void port_mp_sound_endmusic(unsigned int player);   /* Sound::EndMusic(u32) */
int port_mp_sound_changemusicvolume(unsigned int vol, int fade);
int host_setting_peach_jingle(void);
}
#pragma comment(linker, "/alternatename:_port_mp_sound_changemusicvolume=?ChangeMusicVolume@Sound@@YAHIH@Z")  /* catchup1 round 2: the ROM name _ZN5Sound17ChangeMusicVolumeEji is the C++ overload in src */
#pragma comment(linker, "/alternatename:_port_mp_sound_endmusic=?EndMusic@Sound@@YAXI@Z")  /* catchup1 round 2: the ROM name _ZN5Sound8EndMusicEj is the C++ overload in src */
static void port_peach_jingle_tick(void)
{
    enum { PEACH_SEQ = 70, PEACH_CAP = 330 /* fallback when the sequence player cannot be found: the tune is audible about 5.5 s at 60 per second */, PEACH_MAX = 720 /* a stuck player never holds the music longer than this */, PEACH_FADE = 0x15666 };
    static int s_prev_open, s_prev_id = -1, s_age = -1, s_vol = 0x7f, s_player = -1;
    static unsigned s_mask0;
    const int open = data_0209d660 ? 1 : 0;
    const int id = open ? (int)data_0209d6d4 : -1;
    const bool edge = open && !(s_prev_open && s_prev_id == id);
    s_prev_open = open;
    s_prev_id = id;
    if (s_age >= 0 && gCurrentMusicID != PEACH_SEQ) {   /* the level changed, or something else took the music: nothing of ours is left to undo */
        fprintf(stderr, "[peach] music changed under the tune (now %d) after %d frames; dropped\n", gCurrentMusicID, s_age);
        s_age = -1;
    }
    if (s_age < 0) {
        const bool quota = edge && id >= 0x178 && id <= 0x17e;   /* data_ov002_020ff1a0: the 1 / 3 / 8 / 12 / 30 / 50 / 80 star messages */
        const bool level_music = sLayer3Music < 0 && sLayer1Music >= 0 && gCurrentMusicID == sLayer1Music;
        if (quota && host_setting_peach_jingle() && !level_music)
            fprintf(stderr, "[peach] star message 0x%x opened while the music is not the level's own (music %d, level %d, boss layer %d): no tune\n",
                    id, gCurrentMusicID, sLayer1Music, sLayer3Music);
        if (quota && host_setting_peach_jingle() && level_music) {
            s_vol = sMusicVolumeTarget;
            s_mask0 = sd_seq_player_mask();
            s_player = -1;
            _ZN5Sound8SetMusicEjj((unsigned)gLocalPlayerId, PEACH_SEQ);
            if (gCurrentMusicID == PEACH_SEQ) {
                s_age = 0;
                fprintf(stderr, "[peach] star message 0x%x opened: NCS_BGM_PEACH (sequence %d) in place of music %d, volume target was %d\n", id, PEACH_SEQ, sLayer1Music, s_vol);
            }
        }
    }
    if (s_age >= 0) {
        /* what the game wants the music at: its own target while the box is up (read before this tick overrides it), the full level once the box is closed */
        if (!open) s_vol = 0x7f;
        else if (sMusicVolumeTarget != 0x7f) s_vol = sMusicVolumeTarget;
        if (s_player < 0 && s_age >= 1 && s_age <= 20) {
            /* the sound queue starts the sequence a tick after the call. The tune's player is the newly sounding one that starts at offset 0 of
               its own file: a sound effect started on the same tick (the box's own open sound, a button sound) is an entry inside a SEQARC and
               starts further in, so it is never taken for the tune (taking one cut the tune off after 21 frames). */
            const unsigned fresh = sd_seq_player_mask() & ~s_mask0;
            if (fresh) {
                SdSeqPos pos[SD_SEQ_PLAYERS];
                sd_seq_snapshot(pos);
                for (int p = 0; p < SD_SEQ_PLAYERS && s_player < 0; ++p)
                    if ((fresh >> p) & 1u && sd_seq_active(p) && pos[p].startOff == 0) s_player = p;
            }
            if (s_player >= 0) fprintf(stderr, "[peach] the tune is on sequence player %d\n", s_player);
        }
        const bool over = s_player >= 0 ? (s_age > 20 && !sd_seq_active(s_player)) || s_age >= PEACH_MAX : s_age >= PEACH_CAP;
        if (!over) {
            port_mp_sound_changemusicvolume(0x7f, PEACH_FADE);   /* a duck set earlier in the frame is overridden; this is what the sound tick sees */
            ++s_age;
        } else {
            port_mp_sound_endmusic((unsigned)gLocalPlayerId);
            port_mp_sound_changemusicvolume((unsigned)s_vol, PEACH_FADE);
            fprintf(stderr, "[peach] NCS_BGM_PEACH ended after %d frames; level music %d back, volume target %d\n", s_age, gCurrentMusicID, s_vol);
            s_age = -1;
        }
    }
}

/* hal/stage_frame.cpp's Stage::UpdateMessage face lands here. */
extern "C" void port_message_pump(void)
{
    const unsigned char st = data_0209d6bc;
    dScStage_c::UpdateMessage_rom();
    if (data_0209d660 != 0 && data_0209d654 == 0) {
        /* the dialogue arm: Message::Update ran this tick */
        g_text_scroll_tick = (st == 3) ? 1 : 0;
        port_message_flush_engine_a_regs();
    }
    port_peach_jingle_tick();
}
