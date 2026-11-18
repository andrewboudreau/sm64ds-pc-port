/* sCutsceneEndingCakeForMario, hosted rather than byte-copied, with its ONE code
 * pointer seated to a trap.
 *
 * port/tools/ptr_audit.py flagged this one and it was RIGHT: 788 bytes,
 * one relocation, and that relocation points at CODE --
 *     from:0x02089f4c to:0x020c020c (function)
 * i.e. func_ov007_020c020c, which is NOT hosted and lives in ov007,
 * territory this lane is barred from touching. Byte-copying it through
 * romdata.py would put a raw DS address in a word that gets CALLED, which
 * is precisely the sDanceCutsceneTable crash that file's header banks.
 *
 * This lane classified it as a value word on the reasoning that its target
 * had no symbol; that reasoning was WRONG and ptr_audit has better type
 * information. The audit's other five value-word verdicts it CONFIRMS:
 * sCutsceneIntroPeachMarioAppears (the third opening script) reports 6 relocations and ZERO
 * code pointers, which is exactly the record-decode argument.
 *
 * THE POINTER IS SEATED TO A TRAP, not to a DS address and not to a
 * silently wrong host function. sCutsceneEndingCakeForMario is NOT in the opening's
 * four-script chain (020890a0 -> 02088a94 -> 02089af8 -> 020876d4) -- the
 * same sinit patches it for a different cutscene -- so the opening never
 * reaches this word, and anything that does now says what it needs.
 */
#include <cstdio>
#include <cstdlib>
#include "dsstate_seg.h"

extern "C" {

static void intro_dcc_trap(void)
{
    std::fprintf(stderr,
                 "UNHOSTED: sCutsceneEndingCakeForMario+0x180 calls func_ov007_020c020c, which is not hosted. It is ov007 territory and outside the intro-cutscene lane's grant; the opening does not reach this word. See hal/intro_dcc_blob.cpp.\n", 384);
    std::abort();
}

DSSTATE_BEGIN
extern "C" unsigned char sCutsceneEndingCakeForMario[];   /* src/KuppaScript.cpp defines it (catchup1 round 3) */
DSSTATE_END

/* The one seated word. Done at runtime rather than in the initialiser
   because the trap's address is not a compile-time constant an array
   initialiser can take at file scope in C. Runs before anything can read
   it: hal/level_boot.cpp's intro seam calls it alongside the sinit. */
void port_intro_seat_dcc(void)
{
    static int done;
    if (done)
        return;
    done = 1;
    unsigned *w = (unsigned *)(sCutsceneEndingCakeForMario + 384);
    if ((unsigned)(*w) < 0x02000000u || (unsigned)(*w) >= 0x02400000u) return;  /* PORTSYNC3: a mount word that is not a DS code address: the class TU owns the live table */
    if (*w != 0x020c020cu) {
        std::fprintf(stderr, "FATAL: sCutsceneEndingCakeForMario+0x180 holds %08x, the ROM says 020c020c -- WRONG BYTES\n", *w);
        std::abort();
    }
    *w = (unsigned)(size_t)&intro_dcc_trap;
}

}  /* extern "C" */

/* catchup1 round 3: src/KuppaScript.cpp defines sCutsceneEndingCakeForMario. The C name reaches that storage: */
#pragma comment(linker, "/alternatename:_sCutsceneEndingCakeForMario=?sCutsceneEndingCakeForMario@@3PAEA")
