#!/usr/bin/env python3
"""The quiet launch and verdict lines the proof tools share (save_proof, scene_bootpair_proof,
stage_pause_proof, stage_seat_proof, hud_initres_proof).

Every child is spawned the way port/tools/battery.py spawns one: CREATE_NO_WINDOW so no console
appears, STARTUPINFO SW_SHOWMINNOACTIVE so the game window starts minimized and unactivated, and
SM64DS_NO_FOCUS / SM64DS_MINIMIZED / SM64DS_VOLUME=0 in the environment so walk_window keeps its own
half of the same rule (nothing visible, nothing focused, nothing audible).
"""

import os
import subprocess

NO_CONSOLE = getattr(subprocess, "CREATE_NO_WINDOW", 0)
if hasattr(subprocess, "STARTUPINFO"):
    SI_MIN = subprocess.STARTUPINFO()
    SI_MIN.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    SI_MIN.wShowWindow = 7          # SW_SHOWMINNOACTIVE
else:
    SI_MIN = None


def env_base(root, run_dir, name=None):
    """The environment for one proof run: no inherited SM64DS_* knob, the asset root, faults fatal, no
    dialog, quiet, level 1 (castle grounds, the level these proofs were measured on) and a TEMP of the
    run's own. name only labels the caller's run; it is not passed to the game."""
    e = dict(os.environ)
    for k in list(e):
        if k.startswith("SM64DS_"):
            e.pop(k)
    e["SM64DS_ASSET_ROOT"] = root
    e["SM64DS_FAULTS_FATAL"] = "1"
    e["SM64DS_NO_DIALOG"] = "1"
    e["SM64DS_NO_FOCUS"] = "1"
    e["SM64DS_VOLUME"] = "0"
    e["SM64DS_MINIMIZED"] = "1"
    e["SM64DS_LEVEL"] = "1"
    e["TEMP"] = os.path.join(run_dir, "tmp")
    e["TMP"] = e["TEMP"]
    os.makedirs(e["TEMP"], exist_ok=True)
    return e


def spawn(exe, cwd, env, logpath):
    """Start the game with stdout and stderr in one log (the selftest line goes to stdout, the rest to
    stderr)."""
    log = open(logpath, "wb")
    p = subprocess.Popen([exe], cwd=cwd, env=env,
                         stdout=log, stderr=subprocess.STDOUT,
                         creationflags=NO_CONSOLE, startupinfo=SI_MIN)
    p._logfile = log
    return p


def finish(p, timeout):
    try:
        rc = p.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        p.kill()
        rc = -9
    p._logfile.close()
    return rc


def run_one(exe, cwd, env, logpath, timeout=600):
    return finish(spawn(exe, cwd, env, logpath), timeout)


def text(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


VERDICTS = []


def verdict(ok, line):
    VERDICTS.append(("PASS" if ok else "FAIL") + "  " + line)
    print(VERDICTS[-1], flush=True)
    return ok
