#!/usr/bin/env python3
"""volume_check.py: the WAV dump is taken AFTER the host master volume.

Why the audiohq rows launch NO_AUDIO games at SM64DS_VOLUME=100 (audiohq_common.launch): the
mixer applies SM64DS_VOLUME inside sd_mix_render, before the dump taps the buffer, so volume 0
records digital silence (-120 dB). Runs the Linux host harness at 0 and at 100 and checks both.
usage: python dev/volume_check.py      (needs g++; builds into a temp folder)
"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import wavlib as W  # noqa: E402

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))) + "/..")  # port/tools, for decomp_root
import decomp_root  # noqa: E402
T = os.path.join(decomp_root.PORT_DIR, "tests", "audiohq")




if __name__ == "__main__":
    sys.exit(main())
