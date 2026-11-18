"""The HEADROOM's evidence: the census, the timing, the seam, the arms.
Read port/ntr/ppu_sub.cpp's head_paint block for what the headroom is and
tools/gapproof.py for the runner underneath this. Every claim here is a number
off a real run, for the reason gapproof's own header gives: the thing under test
is a picture, and on this machine a windowed capture is not reproducible run to
run, so "look at it" is not a check.

  python port/tools/headroom.py census   the per-frame engine-A OAM census, the
                                         objects that reach above the top screen
                                         and how much of each one the strip
                                         shows against how much the top screen
                                         was already showing
  python port/tools/headroom.py timing   when each object first becomes VISIBLE
                                         with the headroom and when it would
                                         have without it, off one run
  python port/tools/headroom.py seam     the strip's fill against the top
                                         screen's first real row, per column
  python port/tools/headroom.py arms     the A/B on one binary: headroom on,
                                         headroom off, and the containment set
  python port/tools/headroom.py touch    the stylus inverse over the four bands
  python port/tools/headroom.py shots F  the before/after crops at frame F
  python port/tools/headroom.py evidence the gapped top rows against the strip,
                                         with the background-only numbers

THE HEADROOM IS OFF BY DEFAULT and every step here that wants it asks for it
measurement that made it opt-in: there is no background content above the top
screen in either arm on any frame, so the strip can only ever be invented fill
sitting over the picture's own top edge. The steps are kept because they are the
measurement, not because the feature is.

THE TWO ARMS COME OFF ONE BINARY, which is what
notes/port-selftest-bmp-gate.md requires before two BMPs may be compared at
all.

THE CENSUS IS A WINDOWED RUN AND THE CAPTURES ARE NOT. A headless scene run
composes ONCE, at the end, when the capture asks for the image, so it has
exactly one census frame -- which makes it the right mode for every claim about
a finished picture and useless for a claim about WHEN something appeared.
SM64DS_SCENE_WINDOW=1 runs the real frame loop and composes every frame. It
flashes a real window; that is expected.
"""
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gapproof as G
import decomp_root  # noqa: E402

OUT = os.environ.get("HEADOUT",
                     os.path.abspath(os.path.join(decomp_root.PORT_TREE, "runs",
                                                  "mg5", "out", "lakitu")))
os.makedirs(OUT, exist_ok=True)

SCENE = 368
SCREEN_H = 384          # the 2x tier's top screen, host rows

# [headroom] A oam 11 a0=03c2 a1=8249 a2=1010 32x32 dbl at (73,-62): box
#            reaches 34 row(s), 190 pixel(s) drawn, ALL OF IT OFF-SCREEN
ENT = re.compile(
    r"\[headroom\] (\w+) oam\s*(\d+) a0=(\w{4}) a1=(\w{4}) a2=(\w{4}) "
    r"(\d+)x(\d+)(.*?) at \((-?\d+),(-?\d+)\): box reaches (\d+) row")
# [headroom] A oam 11 split: 190 px in the strip, 0 px on the screen
SPLIT = re.compile(r"\[headroom\] (\w+) oam\s*(\d+) split: (\d+) px in the "
                   r"strip, (\d+) px on the screen")
# [headroom] f1 strip 32 DS row(s) ...: 372 DS pixel(s) drawn, 24 of 512 ...
FRAME = re.compile(r"\[headroom\] f(\d+) strip (\d+) DS row\(s\).*?: (\d+) DS "
                   r"pixel\(s\) drawn, (\d+) of (\d+) column")


def run(tag, frames, settings=None, extra=None, windowed=False):
    """One run, its capture and its whole log, both kept."""
    bmp = os.path.join(OUT, "%s.bmp" % tag)
    env = dict(extra or {})
    if windowed:
        env["SM64DS_SCENE_WINDOW"] = "1"
    r = G.scene_run(SCENE, frames, bmp, settings=settings, extra_env=env)
    text = r.stdout + r.stderr
    with open(os.path.join(OUT, "%s.log" % tag), "w") as f:
        f.write(text)
    return r, text, (bmp if os.path.exists(bmp) else None)


def parse(text):
    """[(frame, {oam: entry})], in frame order. An entry carries its decoded
    geometry and the two pixel counts the strip pass split it into."""
    frames = []
    cur = {}
    for ln in text.splitlines():
        m = ENT.search(ln)
        if m:
            cur[int(m.group(2))] = {
                "eng": m.group(1), "oam": int(m.group(2)),
                "a0": m.group(3), "a1": m.group(4), "a2": m.group(5),
                "w": int(m.group(6)), "h": int(m.group(7)),
                "kind": m.group(8).strip(), "x": int(m.group(9)),
                "y": int(m.group(10)), "box_rows": int(m.group(11)),
                "strip": 0, "screen": 0}
            continue
        m = SPLIT.search(ln)
        if m:
            e = cur.get(int(m.group(2)))
            if e:
                e["strip"] = int(m.group(3))
                e["screen"] = int(m.group(4))
            continue
        m = FRAME.search(ln)
        if m:
            frames.append((int(m.group(1)), cur, int(m.group(3)),
                           int(m.group(4)), int(m.group(5))))
            cur = {}
    return frames


def ident(e):
    """An object's IDENTITY across frames: its attribute words minus position.

    Tile, palette, size, shape and mode; not x and not y. Two frames of the
    same object agree on all of it and two different objects drawn from one
    template do too, which is why the census below reports the group rather
    than pretending to track an individual."""
    a0 = int(e["a0"], 16) & ~0x00FF      # drop y
    a1 = int(e["a1"], 16) & ~0x01FF      # drop x
    return (a0, a1, e["a2"], e["w"], e["h"], e["kind"])












# ---- the crops --------------------------------------------------------------
#
# BOTH ARMS AT ONE FRAME, cropped to the top of the picture, with the SAME WORLD
# ROW on the same output row: the off arm is padded by the headroom's height,
# because that is exactly the picture it is missing. The pad is magenta so
# nobody can mistake it for a fill this feature draws.
#
# A HEADLESS RUN COMPOSES ONLY ITS LAST FRAME, which is what makes this exact:
# asking for N frames gives the picture at frame N and no other, so a crop is a
# named frame rather than "somewhere around there".
SHOTS = os.path.join(OUT, "shots")
PAD = (255, 0, 255)          # the rows the off arm does not have


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
    step = sys.argv[1] if len(sys.argv) > 1 else "census"
    n = int(sys.argv[2]) if len(sys.argv) > 2 else None
    if step == "shots":
        for f in [int(a) for a in sys.argv[2:]] or [83, 202, 234, 300]:
            shot(f)
        raise SystemExit(0)
    fn = {"census": census, "timing": timing, "seam": seam, "arms": arms,
          "touch": touch, "evidence": evidence}[step]
    fn(n) if n else fn()
