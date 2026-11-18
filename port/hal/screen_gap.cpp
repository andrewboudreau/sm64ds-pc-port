/* The screen gap's layout owner. See port/hal/screen_gap.h for what the gap is
   and why the layout is one struct rather than five copies of a subtraction. */

#include "hal/screen_gap.h"

#include "hal/host_settings.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

/* THE GAME'S OWN WORD, and it is read through the mount rather than hosted a
   second time. build/port/host-src/ov004_syms.c defines sMgTopScreenYOffset as
   four bytes of real packed ov004 storage at DS 0x020beb6c, with a PACK BROKEN
   check on its offset, and every ROM body that touches the gap -- the setter
   Mg_SetTopScreenYOffset, the reader Mg_GetTopScreenYOffset, RenderOamBothScreens,
   _ZN11dScMgBase_c11PaintSquareEiiii -- reaches it there. Hosting a second copy here would put
   this file's idea of G beside the game's and let them drift apart silently,
   which is the exact defect hal/scene_mg_flower.cpp's note records for the two
   words either side of it.

   AS BYTES, NOT AS AN int. The mount declares it `u8 [4]` and a C++ `extern int`
   of the same name is a different type for the same address; MSVC's C linkage
   makes that link, but the declaration would be a second opinion about the
   storage rather than a reader of it. Four bytes, little endian, assembled
   here. */
extern "C" unsigned char sMgTopScreenYOffset[4];

/* the dual-OAM flag and the compositor's seam hook; see the layout below */
extern "C" unsigned char data_0209e660;
extern "C" void hal_oam_source_a_live(unsigned addr);
static inline int data_0209e660_ref(void) { return data_0209e660 != 0; }

namespace {

int g_have;
int g_raw = -1;              /* the last G read out of the game */
int g_scene = -2;            /* the scene the last latch was for */
int g_head = -1;             /* the headroom the last layout was built with */
int g_shift = -1;            /* the OBJ display shift the last layout used */
int g_setgen_l = -1;         /* host_settings_gen at the last latch: the live
                                re-read steps it when fill/colour/peek move,
                                which changes the layout while every other
                                key input stays equal */
unsigned g_gen;              /* steps whenever the layout's shape changes */
ntr::StackLayout g_lay;

int obj_shift_ds(void);
int obj_layer_shift_ds(void);
int headroom_ds(void);
int main_shadow_is_e674(void);

int read_raw(void)
{
    return (int)((unsigned)sMgTopScreenYOffset[0] |
                 ((unsigned)sMgTopScreenYOffset[1] << 8) |
                 ((unsigned)sMgTopScreenYOffset[2] << 16) |
                 ((unsigned)sMgTopScreenYOffset[3] << 24));
}

/* THE HEADROOM THE LAYOUT SHOULD CARRY RIGHT NOW, in DS rows.
 *
 * IT IS ZERO, AND IT IS ZERO BY DEFAULT BECAUSE THE FEATURE IS WRONG. The
 * strip above the top screen was built on a premise that has since been
 * measured and does not hold, so it is opt-in behind
 * a headroom switch, and nothing turns it on. What follows is the
 * measurement, because the next lane to look at this needs the numbers and not
 * the story.
 *
 * THE PREMISE. With G zeroed the top engine takes world y to y + 192
 * instead of y + 224, and the world's top 32 rows -- -224..-193 -- fall above
 * the top screen's first row. The strip was meant to give those rows back.
 *
 * WHAT IS ACTUALLY IN THEM. Nothing. Measured on scene 368 over 400 frames in
 * BOTH arms with SM64DS_ENGA_LAYERS (hal/message_compositor.cpp), sampling each
 * enabled engine-A background through the game's own sampler:
 *
 *   BG3  32x32 tiles, vofs 0    engine rows -32..-1 are map rows 224..255,
 *                               0 of 8192 texels opaque
 *                               engine rows 0..31 are map rows 0..31,
 *                               8192 of 8192 texels opaque
 *   BG2  64x32 tiles, ofs(256,64)  engine rows -32..-1 are map rows 32..63,
 *                               0 of 8192 opaque, and 0 of 8192 opaque on the
 *                               screen as well: BG2 is enabled and contributes
 *                               no pixel anywhere on any frame
 *
 * and the scroll registers are IDENTICAL in the two arms on every frame, so the
 * background does not follow G and there is no other window to look in.
 *
 * SO THE ARTWORK THE STRIP WAS SUPPOSED TO SHOW IS NOT ABOVE THE SCREEN. It is
 * ON the screen, at engine rows 0..31, fully opaque, and it is the same picture
 * in both arms: with the OBJ layer masked off, the gapped image's rows 0..63
 * and the zero-G-plus-headroom image's rows 64..127 differ in 0 of 32768
 * pixels. The strip's own 64 rows differ from that artwork in 24240 of 32768.
 * A player sees a band of invented fill sitting on top of the picture's own top
 * edge, which is what this was reported as.
 *
 * WHAT THE REAL DEFECT IS, since the strip is not it. Zeroing G moves the
 * OBJECTS 32 rows up the screen and leaves the BACKGROUND where it is, so
 * everything the game draws is 32 rows out of register with the artwork it was
 * drawn against: Lakitu, which the ROM puts 32 rows below the beam, ends up
 * flat against the top edge and loses rows off it. Adding image ABOVE the
 * artwork cannot fix that, because the artwork is the top of the picture.
 *
 * THAT WAS THE OBJECT SHIFT, and it is built and it is ALSO OFF. obj_shift_ds
 * below carries its own measurement: only ONE sprite identity of the twelve on
 * this scene follows G, and moving the whole OBJ layer puts the other eleven,
 * the score rows and the top screen's own artwork, 32 rows down a screen they
 * were never displaced from. Both fixes for this report are now in the tree
 * behind switches, because both are measurements and neither is an
 * improvement. Read obj_shift_ds for what would be.
 *
 * WHEN IT IS ON (the opt-in), the value is the captured head word, the G the running
 * scene's own InitResources wrote, captured by the latch below before it stored
 * zero over it, gated on the mod being engaged FOR THE SCENE NOW RUNNING --
 * the engagement test's own answer, because the flag alone keeps reading 1 after
 * the minigame that set it has ended and a headroom that outlived its scene
 * would size the next one's window wrong. Read once.
 */
int headroom_ds(void)
{
    return 0;
}

/* THE TOP ENGINE'S OBJ DISPLAY SHIFT, in DS rows.
 *
 * IT IS ZERO, AND IT IS ZERO BY DEFAULT BECAUSE THE FEATURE IS WRONG, which is
 * the second time that sentence has been written in this file about a fix for
 * the same report. Read headroom_ds above for the first. The mechanism is whole
 * and it works exactly as designed; the design rests on a premise that has now
 * been measured and does not hold. What follows is the measurement, because the
 * next lane needs the numbers and not the story.
 *
 * THE IDEA. Zeroing G takes the top engine's submission from world y + 0xc0 + G
 * to world y + 0xc0, so a sprite lands G rows higher up a background that did
 * not move with it. Adding G back at the raster would put every sprite on the
 * row the ROM draws it on, and the image would gain G rows below the top screen
 * for the sprites that then run off its bottom edge. That is what this builds:
 * hal/message_compositor.cpp adds it to the entry's y, ntr/ppu_sub.cpp's
 * hinge_paint draws the overflow, and ntr/ppu.h carries the geometry.
 *
 * THE PREMISE IS THAT THE WHOLE OBJ LAYER MOVES WITH G. It does not. Only the
 * submissions that go through the framework's own OAM router do; the rest are
 * placed in SCREEN space and are at the same engine row in both arms. Measured
 * on scene 368 over 300 frames, one headless run per arm, with
 * SM64DS_OBJSHIFT_TRACE (port/tools/objshift.py register), reporting gap-on's
 * engine row minus zero-G's for every entry the two arms share:
 *
 *   a2=1010  32x32 dbl    116 sightings   +32 on 116 of them
 *   a2=211c  32x64 dbl    699 sightings     0 on 699 of them
 *   a2=211c  32x64          4 sightings     0 on 4
 *   a2=e2d7/e2db/e2dc     299 each          0 on every one
 *   a2=d280/e2df          598 / 299         0 on every one
 *   a2=8256/825a/825e/a254  226 each        0 on every one
 *
 * ONE identity of twelve follows G. The other eleven are the top screen's own
 * furniture: the score and timer rows parked at engine y 4 and y 168, and the
 * three 32x64 double-size entries at x 0, 64 and 128 that sit at engine y -32
 * in BOTH arms. A blanket shift moves all twelve, so it puts the score readout
 * into the band and the artwork 32 rows down its own screen.
 *
 * AND THE PICTURE SAYS THE SAME THING. Top screen, 512x384, gap-on against each
 * zero-G arm at frame 300:
 *
 *   gap-on vs zero-G,  shift OFF    2940 of 196608 pixels differ, rows 0..99
 *   gap-on vs zero-G,  shift ON    28152 of 196608 pixels differ, rows 0..177
 *                                  and 336..367
 *
 * The unshifted zero-G top screen is ALREADY the ROM's picture almost
 * everywhere; what is wrong with it is 1.5% of one screen, and it is the routed
 * sprite alone. Turning the shift on multiplies the disagreement by ten.
 *
 * THERE IS A SECOND COST THE SHIFT CANNOT TOUCH, found in the same run.
 * OAM::Render culls at submission with `if (y + h < 0) return`, and a routed
 * sprite's y is G lower in this arm, so 215 submissions over those 300 frames
 * reached OAM in gap-on and reached it in NO form under the mod. A display
 * shift can only move what was submitted.
 *
 * WHAT WOULD BE RIGHT, stated precisely because it is buildable and this is
 * not. The shift has to be per entry, applied to the submissions the
 * framework's G-adding routers made and to no others. Those are
 * Mg_RenderSpriteBothScaled, Mg_RenderSpriteBothPalMtx, Mg_RenderSpriteBothMtx,
 * Mg_RenderSpriteBoth and RenderOamBothScreens, all of which reach
 * OAM::Render with `a2 + 0xc0 + G` while every other submitter passes a plain
 * engine row. Nothing in the finished OAM distinguishes the two, so the port
 * cannot tell them apart at the raster: the routed set has to be recorded AT
 * THE CALL, and that is a hook at those five bodies rather than anything the
 * display side can derive. A table of attribute words would be a guess with a
 * measurement standing behind it today and nothing standing behind it the first
 * time a sprite changes tile.
 *
 * SO IT IS OPT-IN, behind an object-shift switch, and nothing turns it on.
 * The switch stays so an A/B of the picture is one binary, which is what
 * notes/port-selftest-bmp-gate.md requires before two BMPs may be compared at
 * all, and so the next lane can put the per-entry version behind the same
 * geometry without rebuilding the band, the layout or the proofs.
 *
 * WHEN IT IS ON, the value is the captured head word: the G the running scene's own
 * InitResources wrote, captured by the latch below one statement before it
 * stores zero over it, gated on the mod being engaged FOR THE SCENE NOW RUNNING
 * because the flag alone keeps reading 1 after that minigame has ended.
 *
 * READ FROM TWO PLACES and it must give both the same answer within a frame:
 * the layout below sizes the band from it and the engine A compositor shifts
 * its raster by it, so this reads the captured word and the engagement test
 * rather than caching an answer of its own. */
int obj_layer_shift_ds(void)
{
    static int on = -1;
    return 0;
}

/* THE PER-ENTRY CORRECTION, in DS rows, and THIS ONE IS THE DEFAULT.
 *
 * WHAT IT IS. The same G_rom the layer shift adds, added to the submissions the
 * framework's five G-adding OAM routers make and to NOTHING else. Read the
 * router hook below for how the routed set is recorded; read obj_layer_shift_ds
 * above for the measurement that says the set matters. In one line: eleven of
 * the twelve sprite identities on scene 368 are placed in screen space and are
 * at the same engine row whether the framework word reads 32 or 0, so the fix
 * has to be able to tell the twelfth from them, and the only place that
 * distinction exists is at the call.
 *
 * IT IS ON BY DEFAULT, which the two features before it were not, and the
 * difference is that this one is measured RIGHT rather than measured wrong.
 * Its switch set to 0 turns it off and gives back the picture the mod
 * composed before this lane, on the same binary, which is what
 * notes/port-selftest-bmp-gate.md requires before two BMPs may be compared at
 * all.
 *
 * AND IT STANDS DOWN FOR THE LAYER ARM. Both mechanisms put G_rom back and they
 * would put it back twice; more to the point, an A/B is only readable if it is
 * one mechanism against the other. With the object-shift switch on the layer
 * arm is the whole answer and this is zero, which is the arm lane OBJSHIFT
 * measured and the arm its numbers still describe.
 *
 * READ FROM TWO PLACES within a frame -- the router hook adds it at submission,
 * the layout sizes the band from it -- so it reads the captured word and the
 * engagement test rather than caching an answer of its own. */
int per_entry_ds(void)
{
    /* OPT-IN NOW, like the other two arms. The correction and the band it
       needs were one regime; the owner's verdict retired that regime for the
       plain edge-to-edge picture, so the default carries no correction and
       no band. Its switch set to 1 brings the banded arm back for
       measurement on one binary. */
    static int on = -1;
    return 0;
}

/* THE BAND'S WORLD ROWS, which both arms need and which is what the LAYOUT is
   told. The two arms differ in where the correction is applied, not in the
   shape of the image it needs: either way the rows a corrected sprite runs off
   the top screen's bottom edge into are world -G_rom..-1, and either way they
   go between the halves. */
int obj_shift_ds(void)
{
    const int pe = per_entry_ds();
    return pe ? pe : obj_layer_shift_ds();
}

}  // namespace




int hal_screen_gap_raw(void) { return read_raw(); }


const ntr::StackLayout *hal_screen_layout(void)
{
    const int raw = read_raw();
    /* MinigameGap false means G is treated as ZERO FOR LAYOUT, and only for
       layout: the word itself is not written, so the game's sprite submission
       still runs at a2 + 0xc0 + G and objects still vanish at the seam. That is
       precisely what the setting promises -- "remove the gap" gives back the
       picture the port had before this feature, jump and all. */
    int want_gap = host_setting_minigame_gap() ? raw : 0;
    /* ONE HINGE, OWNER'S RULING (2026-08-26). Games write their own G --
       the dScMgD3DBase_c four write 16, most others 32 -- and this layout
       used to draw each game's number, so the visible gap changed size per
       game. The owner tested it directly: paused Trampoline Time with Mario
       inside the hinge, and with a 16-row band his feet barely cleared the
       bottom while nothing showed at the top -- the 3D crossing hides MORE
       rows than the 2D G word says, so drawing 16 was not faithful to that
       family's own physics either. His order: one gap, 32 DS rows (64 host),
       every game. DISPLAY ONLY -- the game's word is not written, its sprite
       submission still runs at a2 + 0xc0 + its own G, so a 2D sprite crossing
       the seam in a G=16 game lands 16 rows early against this band. If that
       is ever visible in play it is a report for the owner, not a reason to
       quietly revert this. */
    if (want_gap)
        want_gap = 32;
    /* THE SHIFT, AND THE BAND IT NEEDS. See obj_shift_ds above. With the mode
       engaged the game's own G reads zero, so `want_gap` is zero and there is
       no band; the shifted objects need one, because the rows they are pushed
       into past the top screen's bottom edge are world -G_rom..-1 and those
       rows have to be somewhere in the image. They go where the hinge goes,
       between the halves, so the band's height is the shift and the image is
       192 + G_rom + 192 DS rows, which is gap-on's own shape.

       THE TWO CANNOT BOTH BE NON-ZERO. `want_gap` is the game's G, which the
       mod has zeroed before anything can engage the shift, so this is a choice
       between a real gap and a shifted band and never a sum. */
    const int shift = obj_shift_ds();
    const int want = shift ? shift : want_gap;
    /* THE HEADROOM, and it is a DISPLAY answer to a SIMULATION cost. See the
       StackLayout note in ntr/ppu.h: with G zeroed, the game's own fixed
       constants keep placing actors in a world 192 + G_rom + 192 rows tall, and
       the top G_rom rows of it fall above the top screen. Giving the IMAGE
       those rows back shows them without touching a constant or the zeroed
       word. It is asked for ONLY while the mod is engaged for the scene now
       running, so every other layout in the program carries a zero.

       IT IS OFF UNLESS THE HEADROOM SWITCH ASKS FOR IT, and headroom_ds
       above carries the measurement that made it opt-in: there is nothing above
       the top screen to draw, in either arm, on any frame. The switch stays so
       an A/B of the picture is one binary, which is what
       notes/port-selftest-bmp-gate.md requires before two BMPs may be compared
       at all. */
    const int head = headroom_ds();
    const int seam = 0;
    /* POWCNT1's DISPLAY SWAP, read live and set on both paths out of here for
       exactly the reason the seam flag is: it is not in the latch key, and it
       is not a per-scene fact at all. GBATEK, POWCNT1 (0x04000304) bit 15:
       0 sends display A to the LOWER screen, 1 to the upper one.

       PER FRAME AND NOT PER SCENE. The dScMgD3DBase_c family's slot 24
       (_ZN14dScMgD3DBase_c16OnGraphCallback2Ev) TOGGLES this bit every frame -- one camera view
       rendered live on the screen the bit names, the other showing the display
       capture unit's copy of the previous frame -- so a value cached at the
       G-latch would be one frame's answer serving a whole minigame. Scene 377
       (Snowball Slalom) clears it once at InitResources and leaves it clear;
       both shapes fall out of reading it here.

       EVERY OTHER SCENE LEAVES IT SET, and hal/sub_screen.cpp's bring-up sets
       it, so this reads 0 for them and ntr::ppu_compose_stacked composes
       exactly what it composed before the bit was read at all. */
    const int main_lower =
        (*(volatile unsigned short *)0x04000304 & 0x8000) ? 0 : 1;
    /* THE SEAM'S FRAME ALIGNMENT, set every frame for the same reason the
       seam flag is: it must die with the scene. With the mod engaged the two
       screens are one picture, so the top screen reads the game's WORKING
       shadow -- the block OAM::Load is about to upload -- instead of the
       hardware OAM it uploaded last frame; otherwise a falling object is one
       frame short exactly at the join. Everywhere else this is zero and the
       top screen draws what it always drew. */
    /* AND THE SHIFT IS PART OF THE LATCH, for the reason the headroom is: it
       cannot move without `want` moving today, and a cache whose key is a
       subset of its inputs is one edit away from serving a stale answer. */
    /* THE LAYER TERM, set on both paths out of here for the reason the seam
       flag is: it is not in the latch key, so a cached layout has to be told it
       too. It is clamped to the band stack_layout actually built rather than to
       the number asked for, so the two can never disagree about rows that do
       not exist. Zero in the per-entry default, which is every run that does
       not ask for the falsified arm. */
    const int raster = obj_layer_shift_ds();
    const int setgen = host_settings_gen();

    const int was_h = g_have ? g_lay.h : 0;
    g_raw = want;
    g_head = head;
    g_shift = shift;
    g_setgen_l = setgen;

    const int mode = host_setting_gap_fill_mode();
    /* PEEK IS OFF IN THE SHIFTED MODE, decided here so that the layout every
       consumer reads already carries the answer and no pass has to ask the
       question a second time. Peek shows what the engines put in rows nobody
       displays; under the shift the top engine displays those rows itself, so
       there is nothing left for peek to reveal and a second rendering of the
       same objects is all it could add. It also drops the band to a black
       backdrop, which would replace the fill the picture is supposed to share
       with gap-on mode. */
    const int peek = shift ? 0 : host_setting_gap_peek();
    /* THE ART IS LOADED HERE, ONCE, and this is the only place that asks for
       it. Both askers are here: the "custom" fill mode wants it with peek off,
       and peek wants it as its backdrop whatever the fill mode says. Asking at
       the latch rather than per frame is what keeps a file read off the frame
       path -- the loader caches on the same (scene, G) pair this does, so the
       second ask in a scene is a pointer compare. */
    const uint32_t *art = 0;

    g_lay = ntr::stack_layout(want, head, shift, mode,
                              host_setting_gap_color(), peek, art);
    g_lay.seam = seam;
    g_lay.world_band = 0;
    g_lay.obj_raster_ds = raster ? g_lay.obj_shift_ds : 0;
    g_lay.main_lower = main_lower;
    /* THE GAME'S OWN G, set on both paths out of here for the reason the seam
       flag and the swap are: it is not in the latch key, so a cached layout
       has to be told it too. Since the ONE HINGE ruling above this is no
       longer `want`, and the band rasters need the difference -- an engine's
       band rows past its own G are the OTHER screen's picture. See the
       field's note in ntr/ppu.h. */
    g_lay.game_g_ds = raw;
    /* and the ambient fill's memory, dropped here for the third time in the
       same sentence: the band now FOLLOWS the two edge rows rather than reading
       them cold, and a decaying glow that outlived its own minigame would light
       the next one's first frames off the last one's picture. */
    ntr::ppu_band_ambient_reset();
    g_have = 1;
    /* THE GENERATION IS THE SHAPE'S, not the latch's. walk_window re-sizes the
       window and re-shapes the DIB header off this counter, and a scene change
       that keeps the same G changes the band's CONTENT and not its size, so
       stepping it there would ask for a resize to the size it already is. */
    if (g_lay.h != was_h) ++g_gen;
    {
        /* THE SUFFIX NAMES WHICH OF THE THREE THINGS HAPPENED TO G, and it has
           to, because since the ONE HINGE ruling `want != raw` has two causes
           and the line used to report both of them as the setting being off.
           Run mg15 lane BAND read `G 16 DS rows (MinigameGap off: layout uses
           0) -> band 64 host rows` off a run with the setting ON and a 32-row
           band on screen -- the line said the gap was gone while it was the
           largest it has ever been. This is the line a report about a
           wrong-looking gap is answered from; it may not be the thing that is
           wrong.

           AND THE HINGE ARM PRINTS `want` RATHER THAN THE LITERAL 32, so the
           day the ruling's number moves this line moves with it instead of
           becoming the second wrong thing the same reader has to notice. */
        char hinge[64];
        std::snprintf(hinge, sizeof hinge,
                      " (ONE HINGE: the layout draws %d)", want);
    }
    return &g_lay;
}

unsigned hal_screen_layout_generation(void)
{
    hal_screen_layout();
    return g_gen;
}

namespace {





}  // namespace





namespace {



}  // namespace







/* ---- THE ROUTER HOOK: which OAM entries were placed in WORLD coordinates ----
 *
 * WHY THERE IS A HOOK AT ALL. The per-entry correction has to be applied to the
 * submissions the framework's five G-adding OAM routers make and to no others,
 * and nothing in the finished OAM tells the two apart: same attribute words,
 * same slot allocation, interleaved indices, and a table of attribute words
 * would be a guess that stopped being true the first time a sprite changed
 * tile. So the routed set is RECORDED AT THE CALL. This is that recording.
 *
 * AND IT IS PORT-SIDE, which was the constraint. src/ and include/ are not
 * touched: port/CMakeLists.txt gives the five router TUs -- and only those five
 * -- two compile definitions each, using the same set_source_files_properties
 * mechanism this port already uses in a dozen places to rename a symbol inside
 * one translation unit. The first renames the router's own definition to
 * <name>__rom so that the wrapper below can carry the real name; the second
 * renames the OAM::Render overload that TU declares extern "C" so that its own
 * calls land on the shims below. Every other caller of OAM::Render in the
 * program, and every other definition of anything, is untouched.
 *
 * WHICH CALL IS THE ROUTED ONE, and this is the whole reason the wrapper takes
 * the router's arguments rather than just setting a flag. Each of the five
 * bodies reaches OAM::Render on more than one path:
 *
 *     Render(0, attr, x, a2 + 0xc0 + G, ...)   the WORLD-frame top-engine
 *                                              submission, the one that carries
 *                                              the G term
 *     Render(0 or 1, attr, x, a2, ...)         the same object handed to the
 *                                              other engine, or to this one in
 *                                              screen space, with no G term
 *
 * so "inside a router" is not enough, and the engine argument is not enough
 * either -- Mg_RenderSpriteBothScaled's two-player branch passes 0 on both. What IS
 * enough is a2 itself: the wrapper has it, it is the third argument of all five
 * bodies, and 0xc0 + G is never zero, so `py == a2 + 0xc0 + G` picks the
 * G-carrying call and can pick nothing else. No band test is duplicated here
 * and no branch of the ROM's is second-guessed.
 *
 * WHERE THE CORRECTION GOES, and there are two answers because OAM::Render has
 * TWO culls: `y + h < 0` and `y > 0xc0`.
 *
 *   AT SUBMISSION, normally. The ROM's own row is py + G_rom, so that is what
 *   is submitted, and OAM::Render then culls on the ROM's number rather than on
 *   one G_rom lower. That is what recovers the entries the mod loses outright
 *   -- 215 of them over 300 frames on scene 368, sprites entering from the top
 *   whose whole box was above row 0 once the mod had moved it up. A display
 *   pass cannot recover those: they never reached OAM.
 *
 *   AT THE RASTER, for the band. A world row in -G_rom..-1 has a ROM row past
 *   0xc0, which is off the bottom of a DS's top screen, and OAM::Render's
 *   `y > 0xc0` throws it away rather than let it reach the band the zero-G
 *   picture draws it in. So when the corrected submission is refused, the
 *   uncorrected one is made instead -- the submission the mod makes today,
 *   which OAM::Render accepts -- and the entry carries a RESIDUAL of G_rom
 *   that the engine A raster and hinge_paint add for that slot alone.
 *
 * THE REFUSAL IS THE TEST, rather than a prediction of it. OAM::Render's cull
 * runs after the cel offset, the affine doubling and the rotation have moved
 * the box, so a test here would be a copy of that arithmetic that could drift
 * from it. A culled call writes nothing and increments no counter, so trying
 * the ROM's row first costs one call and cannot leave a mark behind; the affine
 * allocator deduplicates on the matrix, so even the affine overloads allocate
 * once. And the fallback cannot invent an entry the ROM would not have: a call
 * refused by the BOTTOM cull is refused again at a LOWER y, so the only
 * submission the second call can win is the one the top cull refused.
 *
 * OFF, ALL OF THIS IS TWO STORES. per_entry_ds is zero on every path in the
 * program except a scene the zero-G mode is engaged for, so the
 * wrapper pushes an int and pops it and the shims forward. */

extern "C" {

/* THE REAL OAM::Render OVERLOADS, under their own names. This TU carries no
   rename, so these declarations are the ROM bodies and not the shims. The
   signatures are the ones the router TUs themselves declare, argument for
   argument; the return is spelled as a pointer here because the slot is read
   out of it. */
void *port_sg_oam_render8(int, void *, int, int, int, int,
                                              int, int);
void *_ZN3OAM6RenderEbP7OamAttriiiiP9Matrix2x2(int, void *, int, int, int, int,
                                               void *);
void *port_sg_oam_render10(int, void *, int, int, int,
                                                  int, int, int, int, int);

/* THE MAIN OAM SHADOW, reached AS BYTES for the reason the block over
   sMgTopScreenYOffset gives: the mount owns this storage and a second
   declaration of it as a struct array would be an opinion about the layout
   rather than a use of it. Only its address is taken, to turn the pointer
   OAM::Render returns into a slot index. data_0209e660 is the dual-OAM flag,
   read for the one thing it decides here: which hardware OAM this shadow
   reaches. */
extern unsigned char data_0209e674[];
extern unsigned char data_0209e660;

/* THE MAIN SHADOW'S ENTRY COUNTER, and it is the whole of why this file no
   longer reads OAM::Render's return value. See WHAT THE RETURN VALUE IS WORTH
   below: two of the three overloads never produce the pointer this used to
   test. The counter is the thing OAM::Render actually maintains -- it names the
   next free slot, it is incremented once per entry the call accepts, and it is
   the same word whether the call was culled early or late. Declared `int`, the
   type its own definition and every ROM reader spells.

   ALWAYS THIS COUNTER AND NEVER data_0209e670, because OAM::Render picks its
   pair on `!draw || data_0209e660 == 1` and every G-carrying submission the
   five routers make passes draw = 0. Checked in all five bodies rather than
   assumed. */
extern int data_0209e664;

/* The five router bodies, renamed inside their own TUs by the port's build. */
int func_ov004_020aff38__rom(void *, int, int, int, int, int, int);
void func_ov004_020afdd0__rom(void *, int, int, int, int);
void func_ov004_020b023c__rom(void *, int, int, int, void *);
void func_ov004_020b0380__rom(void *, int, int, void *);
void RenderOamBothScreens__rom(void *, int, int, int, int, void *);

}  /* extern "C" */
#pragma comment(linker, "/alternatename:_port_sg_oam_render8=?Render@OAM@@SAPAUOamAttr@@_NPAU2@HHHHHH@Z")  /* catchup1 round 2: the ROM name _ZN3OAM6RenderEbP7OamAttriiiiii is the C++ overload in src */
#pragma comment(linker, "/alternatename:_port_sg_oam_render10=?Render@OAM@@SAX_NPAUOamAttr@@HHHHHHHH@Z")  /* catchup1 round 2: the ROM name _ZN3OAM6RenderEbP7OamAttriiiiiiii is the C++ overload in src */

namespace {

/* THE ROUTER'S OWN a2, for as long as its body is running. A stack rather than
   a single int, because one router calling another is a shape this file must
   not have an opinion about; four deep is more than the ROM has and the guard
   is what keeps a wrong opinion from becoming a memory bug. */
enum { ROUTE_MAX = 4 };
int g_route_depth;
int g_route_a2[ROUTE_MAX];

struct RoutedCall {
    explicit RoutedCall(int a2)
    {
        if (g_route_depth >= 0 && g_route_depth < ROUTE_MAX)
            g_route_a2[g_route_depth] = a2;
        ++g_route_depth;
    }
    ~RoutedCall() { --g_route_depth; }
};

/* THE CORRECTION THIS CALL EARNS, in DS rows, and zero for every call that is
   not the routed one. See WHICH CALL IS THE ROUTED ONE above: the G-carrying
   submission is the only y that can equal a2 + 0xc0 + G, because 0xc0 + G is
   never zero. */
int routed_adjust(int py)
{
    if (g_route_depth <= 0 || g_route_depth > ROUTE_MAX) return 0;
    if (py != g_route_a2[g_route_depth - 1] + 0xc0 + read_raw()) return 0;
    /* The banded measurement arm keeps its constant correction. Nothing else
       corrects ANYTHING any more. A ramp that eased the correction out
       toward the seam was tried for one evening and measured wrong by the
       owner's own play: the routers submit a crossing object to BOTH engines,
       and any correction applied to the top copy and not the bottom one
       opens a gap of exactly that many sprite rows between the two halves at
       the seam -- the object hits a line, vanishes, and reappears below it.
       The two copies must agree everywhere, so the correction is zero
       everywhere. What a world-anchored sprite standing on screen-anchored
       art needs is its WORLD constant moved, per game, at that game's own
       latch -- the audit the zero-G table's note has demanded all along --
       not a display-side nudge. */
    return per_entry_ds();
}

/* IS THE MAIN SHADOW THE BLOCK ENGINE A WILL BE SHOWING? OAM::Load's own test,
   mirrored rather than assumed: with the dual-OAM flag set it always sends
   data_0209e674 to 0x07000000, and otherwise POWCNT1's display-swap bit
   decides. A mark against the wrong engine's OAM would be a residual applied to
   somebody else's sprite, so this is checked rather than taken on trust. */
int main_shadow_is_e674(void)
{
    if (data_0209e660) return 1;
    return ((*(volatile unsigned short *)0x04000304 & 0x8000) >> 15) == 1;
}

/* THE SPLIT, COUNTED, because a mechanism with two branches and evidence for
   one of them is a mechanism half proven. SM64DS_OBJSHIFT_TRACE=1 names every
   entry the top cull refused at the ROM's row -- the ones the band carries on a
   residual -- and the totals say how often each branch ran. Off, this is two
   increments of a static. */
int routed_trace(void)
{
    static int on = -1;
    if (on < 0) {
        const char *s = std::getenv("SM64DS_OBJSHIFT_TRACE");
        on = s && *s && *s != '0';
    }
    return on;
}

unsigned g_routed_rom, g_routed_band;

void routed_split(int py, int adj, int fell_back)
{
    if (fell_back) ++g_routed_band; else ++g_routed_rom;
    if (!routed_trace()) return;
    std::fprintf(stderr, "[routed] world %d: %s (submitted %d, ROM row %d); "
                 "%u at the ROM's row, %u on a band residual\n",
                 py - 0xc0 - read_raw(),
                 fell_back ? "the top cull refused the ROM's row, so the band "
                             "carries it on a residual"
                           : "submitted at the ROM's row",
                 fell_back ? py : py + adj, py + adj, g_routed_rom,
                 g_routed_band);
}

/* ---- WHAT THE RETURN VALUE IS WORTH, AND WHY THIS COUNTS SLOTS INSTEAD ------
 *
 * THE SHIMS USED TO TEST OAM::Render's RETURN VALUE, on the reading that a
 * culled call returns a null pointer and an accepted one returns the entry. That
 * is true of exactly ONE of the three overloads this file calls, and the other
 * two never produce it:
 *
 *   8 args, ..._Fix12IiEi     src defines it returning OamAttr *, null on every
 *                             cull. THE READING HOLDS HERE and this overload's
 *                             behaviour is unchanged by what follows.
 *
 *   7 args, ...P9Matrix2x2    src defines it returning VOID. Whatever is in the
 *                             return register at each exit is leftover
 *                             arithmetic -- the counter, x+w, y+h -- and it is
 *                             never zero. So `if (r)` was always taken, the
 *                             fallback below it was dead code, and the pointer
 *                             handed to the old routed_mark pointed outside the
 *                             shadow and failed its range test. Entries the top
 *                             cull refused were therefore DROPPED: never
 *                             re-submitted, never marked, never drawn in the
 *                             band.
 *
 *   10 args, ..._Fix12IiES3_ii  the C-linkage name is a forwarder in
 *                             hal/reverse_bridges.cpp that calls the C++ member
 *                             and returns 0 unconditionally. So `if (r)` was
 *                             NEVER taken, the second call ALWAYS ran, and every
 *                             routed submission through Mg_RenderSpriteBoth was
 *                             rendered TWICE -- once at the ROM's row and once
 *                             at the mod's row, 32 rows apart, with no mark on
 *                             either. That is the doubled sprite the player
 *                             reported.
 *
 * SO THE TEST IS THE COUNTER, which is the thing OAM::Render actually maintains
 * and the one signal all three overloads produce identically. data_0209e664
 * names the next free slot in the main shadow; the call increments it once per
 * entry it accepts and leaves it alone on every cull. Reading it either side of
 * the call says whether the submission landed and, exactly, WHICH SLOTS it
 * landed in -- which the 10-argument overload needs anyway, because it walks a
 * list and can accept several entries in one call where the old
 * pointer-and-one-slot reading could only ever record the last.
 *
 * AND EACH ENTRY IS THEN DRAWN EXACTLY ONCE. The second call is made only when
 * the first allocated NOTHING, so the two can never both be in the shadow;
 * whatever the second one allocates is marked with the residual, and the band
 * draws those rows while the top screen's raster clips them. */

int oam_slot_count(void) { return data_0209e664; }

/* MARK THE SLOTS A CALL JUST TOOK, from c0 up to but not including c1. The
   bounds test is the shadow's own 128 entries; a counter outside them is a
   state this file should record nothing about rather than guess at. */
void routed_mark_range(int c0, int c1, int resid)
{
    if (!main_shadow_is_e674()) return;
    for (int s = c0; s < c1; ++s)
        if (s >= 0 && s < 128) ntr::ppu_obj_routed_record(s, resid);
}

}  /* namespace */

extern "C" {

void *port_oam_render_routed_8(int draw, void *obj, int px, int py, int pal,
                               int prio, int scale, int rot)
{
    const int adj = routed_adjust(py);
    if (!adj)
        return port_sg_oam_render8(draw, obj, px, py, pal,
                                                       prio, scale, rot);
    const int c0 = oam_slot_count();
    void *r = port_sg_oam_render8(draw, obj, px, py + adj,
                                                      pal, prio, scale, rot);
    const int c1 = oam_slot_count();
    if (c1 != c0) {
        routed_mark_range(c0, c1, 0);
        routed_split(py, adj, 0);
        return r;
    }
    r = port_sg_oam_render8(draw, obj, px, py, pal, prio,
                                                scale, rot);
    const int c2 = oam_slot_count();
    routed_mark_range(c0, c2, adj);
    if (c2 != c0) routed_split(py, adj, 1);
    return r;
}

void *port_oam_render_routed_mtx(int draw, void *obj, int px, int py, int pal,
                                 int prio, void *mtx)
{
    const int adj = routed_adjust(py);
    if (!adj)
        return _ZN3OAM6RenderEbP7OamAttriiiiP9Matrix2x2(draw, obj, px, py, pal,
                                                        prio, mtx);
    const int c0 = oam_slot_count();
    void *r = _ZN3OAM6RenderEbP7OamAttriiiiP9Matrix2x2(draw, obj, px, py + adj,
                                                       pal, prio, mtx);
    const int c1 = oam_slot_count();
    if (c1 != c0) {
        routed_mark_range(c0, c1, 0);
        routed_split(py, adj, 0);
        return r;
    }
    r = _ZN3OAM6RenderEbP7OamAttriiiiP9Matrix2x2(draw, obj, px, py, pal, prio,
                                                 mtx);
    const int c2 = oam_slot_count();
    routed_mark_range(c0, c2, adj);
    if (c2 != c0) routed_split(py, adj, 1);
    return r;
}

void *port_oam_render_routed_10(int draw, void *obj, int px, int py, int pal,
                                int prio, int sx, int sy, int a8, int a9)
{
    const int adj = routed_adjust(py);
    if (!adj)
        return port_sg_oam_render10(draw, obj, px, py,
                                                           pal, prio, sx, sy,
                                                           a8, a9);
    const int c0 = oam_slot_count();
    void *r = port_sg_oam_render10(draw, obj, px,
                                                          py + adj, pal, prio,
                                                          sx, sy, a8, a9);
    const int c1 = oam_slot_count();
    if (c1 != c0) {
        routed_mark_range(c0, c1, 0);
        routed_split(py, adj, 0);
        return r;
    }
    r = port_sg_oam_render10(draw, obj, px, py, pal,
                                                    prio, sx, sy, a8, a9);
    const int c2 = oam_slot_count();
    routed_mark_range(c0, c2, adj);
    if (c2 != c0) routed_split(py, adj, 1);
    return r;
}

/* THE FIVE WRAPPERS. Each carries the real name every caller in the program
   spells, captures the world row its body is about to route, and hands the call
   straight to the ROM body under its renamed symbol. Nothing else: no test, no
   branch of the router's repeated here, and with the mode off the whole of this
   is a push and a pop. */

int Mg_RenderSpriteBothScaled(void *a0, int a1, int a2, int a3, int a4, int a5,
                        int a6)
{
    RoutedCall rc(a2);
    return func_ov004_020aff38__rom(a0, a1, a2, a3, a4, a5, a6);
}

void Mg_RenderSpriteBoth(void *a0, int a1, int a2, int a3, int a4)
{
    RoutedCall rc(a2);
    func_ov004_020afdd0__rom(a0, a1, a2, a3, a4);
}

void Mg_RenderSpriteBothPalMtx(void *a0, int a1, int a2, int a3, void *a4)
{
    RoutedCall rc(a2);
    func_ov004_020b023c__rom(a0, a1, a2, a3, a4);
}

void Mg_RenderSpriteBothMtx(void *a0, int a1, int a2, void *a3)
{
    RoutedCall rc(a2);
    func_ov004_020b0380__rom(a0, a1, a2, a3);
}

void RenderOamBothScreens(void *a0, int a1, int a2, int a3, int a4, void *a5)
{
    RoutedCall rc(a2);
    RenderOamBothScreens__rom(a0, a1, a2, a3, a4, a5);
}

}  /* extern "C" */
