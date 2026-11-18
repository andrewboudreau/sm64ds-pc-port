"""The PER-ENTRY correction's evidence, arm by arm.
Read THE ROUTER HOOK at the foot of port/hal/screen_gap.cpp for what the
correction is, THE OBJECT SHIFT in port/ntr/include/ntr/ppu.h for the geometry
version of the same idea. This tool is that lane's tool with the arms changed:
its steps are re-run here against the mechanism that replaced the one it killed,
so the two sets of numbers are read off the same probes.

  python port/tools/perentry.py arms       every arm's image size and md5,
                                           including the two containment arms
                                           that must still be the tip's
  python port/tools/perentry.py register   THE ACCEPTANCE: per sprite identity,
                                           gap-on's display row minus each
                                           the count of submissions gap-on makes
                                           that the arm does not
  python port/tools/perentry.py diff       the composed images against each
                                           other by region
  python port/tools/perentry.py crossing   lane STRADDLE's census, verbatim, in
                                           both arms
  python port/tools/perentry.py timing     when the routed sprite first has rows
                                           on the screen, per arm
  python port/tools/perentry.py touch      the stylus inverse over the bands
                                           per identity, in every arm
  python port/tools/perentry.py shots F    the side by side at frame F
  python port/tools/perentry.py full F     one whole composed frame at frame F

THE THREE ARMS, and all three are the same binary, which is what
notes/port-selftest-bmp-gate.md requires before two BMPs may be compared:

                          Squad's own code reads the framework gap word nowhere,
                          so the simulation is bit-for-bit the same in every arm
                          and the only thing that differs is the mapping of
                          world rows onto the two engines.
                          routers' own calls.
                          composed before this lane.

PAIRING IS BY IDENTITY, NOT BY OAM SLOT AND NOT BY POSITION, and both halves of
that are a change from objshift.py's register on purpose.

NOT BY SLOT, because the correction lands before OAM::Render's culls: entries
the mod used to lose now take slots, and every later entry in that frame is
renumbered. A slot-keyed pairing reports the renumbering as a disagreement.

NOT BY POSITION EITHER, because OAM::Render's rotation mixes the two axes --
`x = ((sx * c - sy * s) >> 12) - w / 2` with sy taken from the submitted y -- so
correcting a SPINNING sprite by 32 rows moves its x as well. An x-keyed pairing
reported the routed 32x32 double-size sprite as absent from an arm that had all
385 of its sightings. See ident() for the key that is left: the tile, the
palette and the priority with the decoded size beside them, which is what the
sprite IS. Several of one identity in a frame pair nearest-first on x.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import decomp_root  # noqa: E402

OUT = os.path.abspath(os.path.join(decomp_root.PORT_TREE, "runs", "mg5", "out",
                                   "perentry"))
os.makedirs(OUT, exist_ok=True)
os.environ["OBJSHIFTOUT"] = OUT

import gapproof as G          # noqa: E402
import objshift as O          # noqa: E402

SCENE = O.SCENE
SCREEN_H = O.SCREEN_H
TRACE = O.TRACE
BOMB = O.BOMB                 # a2=1010, the routed 32x32 double-size sprite
LAKITU = O.LAKITU             # a2=211c, what lane LAKITU named Lakitu

GAPON = ("gap on (the ROM)", None, {})

SHOTS = os.path.join(OUT, "shots")


def tagof(name):
    return re.sub(r"\W+", "_", name)


def traced(arm, frames):
    """One traced headless run of an arm; returns [(frame, [entry...])]."""
    name, settings, extra = arm
    env = dict(TRACE)
    env.update(extra)
    _, txt, _ = O.run("trace_%s" % tagof(name), frames, settings=settings,
                      extra=env)
    return O.entries(txt)


# ---- the arms ---------------------------------------------------------------



# ---- the register -----------------------------------------------------------

def kind(e):
    return (" dbl" if "dbl" in e["kind"]
            else (" aff" if e["kind"] else ""))


def ident(e):
    """THE SPRITE, INDEPENDENT OF WHERE IT IS DRAWN, which is the only kind of
    key that can pair two arms of this A/B.

    NOT (a0, a1): attribute 0 carries the y, so a moved sprite has a different
    a0 by construction, and attribute 1 carries the x, which moves WITH the y
    for a rotated entry -- OAM::Render's rotation mixes them (`x = ((sx * c -
    sy * s) >> 12) - w / 2`), so a 32 row correction moves a spinning sprite
    sideways too. Keying on either of those reports the correction as a missing
    sprite, which it is not. a2 is the tile, the palette and the priority, and
    with the decoded size and affine kind beside it that is what the sprite IS.
    """
    return (e["a2"], e["w"], e["h"], kind(e))


def is_reset_entry(e):
    """The OAM RESET PATTERN, which is not a sprite and is not evidence.

    OAM::Reset writes 0x000000c0 into all 128 shadow entries, so every slot the
    frame does not use decodes as an 8x8 tile-0 sprite parked at engine row 192.
    The compositor rasterises them (they are not the disabled bit) and the trace
    prints them, ~115 a frame in every arm, and they are the same 115 in every
    arm. Counted separately below rather than left to drown the table."""
    return e["a0"] == "00c0" and e["a1"] == "0000" and e["a2"] == "0000"


def pair_frame(a, b):
    """[(entry_a, entry_b)] for one frame, paired by identity then by column.

    Where a frame holds several of one identity they are paired NEAREST FIRST
    on x: two bob-ombs of the same tile are told apart by where they are, and a
    correction of 32 rows cannot move one past another."""
    idx = {}
    for e in b:
        idx.setdefault(ident(e), []).append(e)
    out = []
    for e in a:
        q = idx.get(ident(e))
        if not q:
            out.append((e, None))
            continue
        best = min(range(len(q)), key=lambda i: (abs(q[i]["x"] - e["x"]),
                                                 abs(q[i]["d0"] - e["d0"])))
        out.append((e, q.pop(best)))
    return out




# ---- what is cut off at the top ---------------------------------------------



# ---- the picture ------------------------------------------------------------







# ---- the crossing -----------------------------------------------------------



# ---- when the routed sprite arrives -----------------------------------------



# ---- the stylus -------------------------------------------------------------



# ---- the band with something in it ------------------------------------------



# ---- what the routed sprite actually IS -------------------------------------

BOMBLINE = re.compile(r"\[bomb\] f(\d+) G=(-?\d+) slot(\d+) world=\((-?\d+),"
                      r"(-?\d+)\) state=(-?\d+) topsy=(-?\d+) botsy=(-?\d+)")


def identify(frames=200):
    """WHAT a2=1010 IS, off the game's own slot table rather than off a picture.

    hal/scene_mg.cpp's SM64DS_PCH_BALL_TRACE prints dScMgPachinko_c's own
    parachuting bob-omb slots -- 30 of them at scene + 0x4660, world position,
    live flag, state -- and the row the top engine would take each to. The
    per-entry trace prints where the OAM entries really are. If the routed
    identity is the bob-ombs then on every frame the set of `topsy` values from
    the slot table is the set of rows the a2=1010 entries were submitted at, and
    that is a correspondence rather than a resemblance.

    Run with the gap ON, because that is the arm whose rows are the ROM's."""
    env = dict(TRACE)
    env["SM64DS_PCH_BALL_TRACE"] = "1"
    _, txt, _ = O.run("identify_gapon", frames, settings=None, extra=env)
    bombs = {}
    for ln in txt.splitlines():
        m = BOMBLINE.search(ln)
        if m:
            bombs.setdefault(int(m.group(1)), []).append(
                {"slot": int(m.group(3)), "wx": int(m.group(4)),
                 "wy": int(m.group(5)), "top": int(m.group(7))})
    ents = dict(O.entries(txt))
    hit = miss = frames_seen = 0
    sample = []
    agreed = []
    cels = {}
    for f in sorted(bombs):
        live = bombs[f]
        rows = [e for e in ents.get(f, []) if e["a2"] == BOMB]
        if not live and not rows:
            continue
        frames_seen += 1
        # the entry's SUBMITTED row is display minus the shift it carried
        got = sorted(e["d0"] - e["shift"] for e in rows)
        # OAM::Render puts the box at py + the cel offset, which is the same
        # constant for every one of these, so the two lists match up to that
        # one offset. Solve for it once per frame and require it to be equal.
        want = sorted(b["top"] for b in live)
        if len(got) != len(want):
            miss += 1
            if len(sample) < 8:
                sample.append((f, "%d slot(s) live, %d entr(ies)"
                               % (len(want), len(got))))
            continue
        offs = set(g - w for g, w in zip(got, want))
        if len(offs) == 1:
            hit += 1
            o = list(offs)[0]
            cels[o] = cels.get(o, 0) + 1
            if len(agreed) < 6 and want:
                agreed.append((f, "%d bob-omb(s), slot row(s) %s, entry row(s) "
                               "%s, cel offset %d" % (len(want), want, got, o)))
        else:
            miss += 1
            if len(sample) < 8:
                sample.append((f, "offsets %s" % sorted(offs)))
    print("a2=%s against dScMgPachinko_c's own parachuting bob-omb slots, "
          "gap on, %d frames" % (BOMB, frames))
    print("  %d frame(s) where the two agree exactly, %d where they do not"
          % (hit, miss))
    print("  the constant offset between the two, which is the sprite's own cel "
          "offset: %s" % ", ".join("%d on %d frame(s)" % (o, n)
                                   for o, n in sorted(cels.items())))
    print("  AGREEING frames:")
    for f, why in agreed:
        print("    f%-5d %s" % (f, why))
    print("  DISAGREEING frames:")
    for f, why in sample:
        print("    f%-5d %s" % (f, why))
    return hit, miss


if __name__ == "__main__":
    step = sys.argv[1] if len(sys.argv) > 1 else ""
    n = int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2].isdigit() else 0
    if step == "arms":
        arms(n or 300)
    elif step == "register":
        register(n or 400)
    elif step == "crossfull":
        for f in (sys.argv[2:] or ["636"]):
            cross_full(int(f))
    elif step == "identify":
        identify(n or 200)
    elif step == "clipped":
        clipped(n or 400)
    elif step == "diff":
        diff(n or 300)
    elif step == "crossing":
        crossing(n or 1400)
    elif step == "timing":
        timing(n or 400)
    elif step == "touch":
        touch()
    elif step == "shots":
        for f in (sys.argv[2:] or ["300"]):
            shot(int(f))
    elif step == "full":
        for f in (sys.argv[2:] or ["300"]):
            full(int(f))
    else:
        print(__doc__)
