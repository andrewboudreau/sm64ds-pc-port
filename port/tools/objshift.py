"""The OBJECT SHIFT's evidence: the register, the picture, the crossing.
Read THE OBJECT SHIFT in port/ntr/include/ntr/ppu.h for what the mode is, and
tools/gapproof.py for the runner underneath this. Every claim here is a number
off a real run, for the reason gapproof's own header gives: the thing under test
is a picture, and on this machine a windowed capture is not reproducible run to
run, so "look at it" is not a check.

  python port/tools/objshift.py arms      the A/B on one binary: the shift on,
                                          the shift off, gap on, MinigameGap
                                          false, with the image size and md5 of
                                          each
  python port/tools/objshift.py register  every engine-A OBJ entry's engine row
                                          in each arm, frame for frame, over one
                                          headless run of each. THE FALSIFYING
                                          STEP: it reports, per sprite identity,
                                          and the answer is +32 for one identity
                                          and 0 for all the others
  python port/tools/objshift.py diff      the two images against each other, by
                                          region: top screen, band, bottom
                                          screen
  python port/tools/objshift.py crossing   a scripted slingshot launch, windowed,
                                          with the seam straddle census on: the
                                          per-frame table of how much of a
                                          crossing ball each half of the picture
                                          really shows
  python port/tools/objshift.py timing     when a routed sprite enters the
                                          picture, in all three arms, off the
                                          per-entry trace rather than off a
                                          bounding box
  python port/tools/objshift.py touch      the stylus inverse over the bands
  python port/tools/objshift.py shots F    the side-by-side crops at frame F

BINARY, which is what notes/port-selftest-bmp-gate.md requires before two BMPs
may be compared at all. It is off because `register` below falsified it: only
the sprites the framework's own OAM router placed move with G, and the shift
moves the whole layer. Read obj_shift_ds in hal/screen_gap.cpp for the numbers.
These steps are kept because they are the measurement, not because the feature
is.

WHY GAP-ON IS THE GROUND TRUTH HERE. Bob-omb Squad's own code reads the
framework gap word nowhere (the audit is in hal/screen_gap.cpp), so the
simulation is bit-for-bit the same in both arms and the only thing that differs
is the mapping of world rows onto the two engines. The shifted arm's top screen
SHOULD therefore be the gap-on picture exactly, and `diff` is where that was
checked rather than assumed. It is not: 28152 of 196608 pixels differ with the
shift on against 2940 with it off, which is the whole finding.

FRAME COUNT IS PART OF THE PROBE. A headless run composes ONCE, at the end, when
the capture asks for the image, so asking for N frames gives the picture at
frame N and no other. The per-entry trace, however, prints every frame on the
headless path too, because the engine A compositor runs inside the frame loop
and only the COMPOSE waits for the end.
"""
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gapproof as G
import decomp_root  # noqa: E402

OUT = os.environ.get("OBJSHIFTOUT",
                     os.path.abspath(os.path.join(decomp_root.PORT_TREE, "runs",
                                                  "mg5", "out", "objshift")))
os.makedirs(OUT, exist_ok=True)

SCENE = 368
SCREEN_H = 384          # the 2x tier's screen, host rows
SCALE = 2               # host rows per DS row
TRACE = {"SM64DS_OBJSHIFT_TRACE": "1"}

LAKITU = "211c"         # a2 of the 32x64 double-size affine entry, the report
BOMB = "1010"           # a2 of the 32x32 double-size parachuting bob-ombs

# [objshift] f12 oam 11 a0=03c2 a1=8249 a2=1010 32x32 dbl at (73,-62) shift 32
#            display -30..33: 30 above, 34 top, 0 band
ENT = re.compile(
    r"\[objshift\] f(\d+) oam\s*(\d+) a0=(\w{4}) a1=(\w{4}) a2=(\w{4}) "
    r"(\d+)x(\d+)(.*?) at \((-?\d+),(-?\d+)\) shift (-?\d+) "
    r"display (-?\d+)\.\.(-?\d+): (\d+) above, (\d+) top, (\d+) band")
# [hinge] f232 band 32 DS row(s) (world -32..-1, engine rows 160..191): 96 ...
HINGE = re.compile(r"\[hinge\] f(\d+) band (\d+) DS row\(s\).*?: (\d+) DS pix")
# [gapstraddle] f119 slot 0 16x16 at (125,-4 world) bottom half only, drawn
#     from B a0=... at (125,-4 world): before top 0/42 bot 166/166, after
#     top 42/42 bot 166/166, 42 synthesized, total 208/208
STR_DRAWN = re.compile(
    r"\[gapstraddle\] f(\d+) slot (\d+) (\d+)x(\d+) at \((-?\d+),(-?\d+) world\) "
    r"(.*?), drawn from (\w+) a0=(\w{4}) a1=(\w{4}) a2=(\w{4}) at "
    r"\((-?\d+),(-?\d+) world\): before top (\d+)/(\d+) bot (\d+)/(\d+), "
    r"after top (\d+)/(\d+) bot (\d+)/(\d+), (\d+) synthesized, "
    r"total (\d+)/(\d+)")
# [gapstraddle] f121 slot 0 16x16 at (126,-12 world): top no, bottom no -- ...
STR_NONE = re.compile(
    r"\[gapstraddle\] f(\d+) slot (\d+) (\d+)x(\d+) at \((-?\d+),(-?\d+) "
    r"world\): top (\S+ ?\S*), bottom (\S+ ?\S*) -- (.*)")


def run(tag, frames, settings=None, extra=None, windowed=False, keep_log=True):
    """One run, its capture and its whole log, both kept."""
    bmp = os.path.join(OUT, "%s.bmp" % tag)
    env = dict(extra or {})
    if windowed:
        env["SM64DS_SCENE_WINDOW"] = "1"
    r = G.scene_run(SCENE, frames, bmp, settings=settings, extra_env=env)
    text = r.stdout + r.stderr
    if keep_log:
        with open(os.path.join(OUT, "%s.log" % tag), "w") as f:
            f.write(text)
    return r, text, (bmp if os.path.exists(bmp) else None)


def entries(text):
    """[(frame, [entry...])] in frame order, one list per composited frame."""
    frames = {}
    for ln in text.splitlines():
        m = ENT.search(ln)
        if not m:
            continue
        f = int(m.group(1))
        frames.setdefault(f, []).append({
            "oam": int(m.group(2)), "a0": m.group(3), "a1": m.group(4),
            "a2": m.group(5), "w": int(m.group(6)), "h": int(m.group(7)),
            "kind": m.group(8).strip(), "x": int(m.group(9)),
            "y": int(m.group(10)), "shift": int(m.group(11)),
            "d0": int(m.group(12)), "d1": int(m.group(13)),
            "above": int(m.group(14)), "top": int(m.group(15)),
            "band": int(m.group(16))})
    return [(f, frames[f]) for f in sorted(frames)]


# ---- the arms ---------------------------------------------------------------



# ---- the register -----------------------------------------------------------



# ---- the two pictures against each other ------------------------------------



# ---- the crossing -----------------------------------------------------------

# The straddle lane's own probe, kept verbatim: two scripted slingshot launches,
# grab the ball where it rests, drag, release. Reusing it is the point -- a
# different probe would be a different crossing and the two tables could not be
# read against each other.
PROBE = ("200:128:40,201-210:128:180,211-215,"
         "600:128:40,601-610:128:70,611-615")




# ---- the stylus -------------------------------------------------------------



# ---- the crops --------------------------------------------------------------

SHOTS = os.path.join(OUT, "shots")
SEP = (32, 32, 32)


def write_bmp(path, rows):
    h = len(rows)
    w = len(rows[0])
    stride = (w * 3 + 3) & ~3
    data = bytearray()
    for y in range(h - 1, -1, -1):
        line = bytearray()
        for x in range(w):
            r, g, b = rows[y][x]
            line += bytes((b, g, r))
        line += b"\0" * (stride - len(line))
        data += line
    hdr = struct.pack("<2sIHHI", b"BM", 14 + 40 + len(data), 0, 0, 14 + 40)
    hdr += struct.pack("<IiiHHIIiiII", 40, w, h, 1, 24, 0, len(data),
                       2835, 2835, 0, 0)
    with open(path, "wb") as f:
        f.write(hdr + bytes(data))








if __name__ == "__main__":
    step = sys.argv[1] if len(sys.argv) > 1 else ""
    if step == "arms":
        arms()
    elif step == "register":
        register(int(sys.argv[2]) if len(sys.argv) > 2 else 400)
    elif step == "diff":
        diff(int(sys.argv[2]) if len(sys.argv) > 2 else 300)
    elif step == "crossing":
        crossing(int(sys.argv[2]) if len(sys.argv) > 2 else 1400)
    elif step == "touch":
        touch()
    elif step == "shots":
        for f in (sys.argv[2:] or ["300"]):
            shot(int(f))
    elif step == "full":
        for f in (sys.argv[2:] or ["300"]):
            full(int(f))
    elif step == "timing":
        timing(int(sys.argv[2]) if len(sys.argv) > 2 else 400)
    else:
        print(__doc__)
