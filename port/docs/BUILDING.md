# Building the port from a fresh clone

These are the exact steps for the public repo (`port/` beside a `decomp/` submodule), run in
a Windows command prompt (`cmd.exe`) from the repo's top folder. Nothing from the game is in
the repo: steps 2 to 5 make it, once, from your own cartridge dump.

## What you need

- Your own dump of **Super Mario 64 DS, Europe** (`.nds`, SHA-1
  `1367529f2cb23e76ef295cb1727333ae8f0a6cd7`). Other regions do not work.
- Windows 10 or 11.
- Visual Studio 2022 (Build Tools, Community, Professional or Enterprise) with the
  "Desktop development with C++" workload, which brings the 32-bit C++ compiler and the
  "C++ CMake tools for Windows" (CMake and Ninja). `port\tools\vs_env.cmd` finds it through
  `vswhere`, so any edition in any install folder works; without the CMake tools it uses a
  `cmake` and `ninja` on `PATH`.
- Python 3.12 or newer on `PATH`, and Git.
- `dsd` 0.11.0, the ds-decomp toolkit: the Windows binary from
  https://github.com/AetiasHax/ds-decomp/releases (release `v0.11.0`), saved as
  `decomp\tools\bin\dsd.exe`. Only step 3 uses it.

## Steps

In the commands, `C:\path\to\your.nds` stands for the full path of your dump.

1. Clone with the decomp, and install the Python packages:

       git clone --recurse-submodules https://github.com/tangosdev/sm64ds-pc-port.git
       cd sm64ds-pc-port
       pip install ndspy pefile capstone==5.0.9

2. Unpack the code (writes `decomp\extracted\`: `arm9.bin`, `arm7.bin`, `arm9_dec.bin`,
   `overlays\`):

       python decomp\tools\unpack.py "C:\path\to\your.nds"

3. Extract the cartridge's files and tables (writes `decomp\extracted\dsd\`: `header.yaml`,
   `arm9\`, `arm9_overlays\`, `files\` and the rest):

       decomp\tools\bin\dsd.exe rom extract --rom "C:\path\to\your.nds" --output-path decomp\extracted\dsd

4. Make the asset catalog (writes `build\assets\` and `build\generated\` in this repo):

       python decomp\tools\asset_catalog.py generate "C:\path\to\your.nds" --manifest build\assets\files.tsv --handles build\assets\handles.tsv --nitrofs build\assets --file-header build\generated\NitroFileId.h --handle-header build\generated\AssetHandle.h

5. Write the file list the port's test mount reads (`decomp\extracted\dsd\files\files.txt`):

       python port\tools\ntr_manifest.py

6. Build:

       port\build-port.cmd

   The game is `build\port\walk_window.exe`.

7. Check the build the way the project does (it builds first, then runs every test):

       python port\tools\battery.py

   It ends with `battery: ALL GREEN`.

## Where the game finds its files

A developer build reads the catalog from `build\assets\` in this repo and the cartridge
files from the decomp's `extracted\` (`port\hal\extracted_root.h` has the rule:
`<asset root>\extracted` when it exists, else the decomp the build compiled, else
`<asset root>\decomp\extracted`). Nothing needs linking or copying. `SM64DS_ASSET_ROOT`
moves the asset root; `SM64DS_DECOMP_ROOT` (or `-DDECOMP_ROOT=` for CMake) builds against
another decomp checkout.

If `port\` sits inside a decomp checkout instead (the decomp repo's own layout), run the same
commands from that checkout without the `decomp\` prefix: everything lands in its own
`extracted\` and `build\`.
