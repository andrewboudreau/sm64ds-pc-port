// Plain .wav / .ogg audio files read from disk (lane AUDIOHQ1).
//
// Portable: no windows, no game state, so the same code is built into the game
// and into the Linux test harness. It does three jobs:
//
//   1. sd_audio_probe     read a file's header and loop tags, decide if it is usable
//   2. sd_audio_decode_mono   a whole file to one mono PCM16 clip (a replaced WAVE)
//   3. SdSource           a file read forward in small blocks as stereo PCM16, with
//                         loop handling (a replaced MUSIC TRACK). An .ogg stays
//                         compressed in memory and is decoded as it is played, so a
//                         three minute track costs its file size, not 35 MB.
//
// Loop points: WAV `smpl` chunk (first loop; its end is inclusive in the file and
// is made exclusive here), OGG comments LOOPSTART / LOOPLENGTH / LOOPEND
// (LOOPEND is the first frame NOT played, like LOOPSTART + LOOPLENGTH). All in
// sample frames of the file's own rate.
#ifndef SM64DS_PORT_SD_AUDIO_H
#define SM64DS_PORT_SD_AUDIO_H

#include "sdat.h"

#include <string>
#include <vector>







// ---- a replaced wave, as an SWAV record and a rebuilt SWAR ----------------






// Hermite (Catmull-Rom) interpolation of y0..y1 at fraction f, given the
// neighbours ym1 and y2. Shared by the mixer.
static inline float sd_hermite(float ym1, float y0, float y1, float y2, float f)
{
    const float c1 = 0.5f * (y1 - ym1);
    const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * f + c2) * f + c1) * f + y0;
}

#endif
