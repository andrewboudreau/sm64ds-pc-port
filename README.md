# SM64DS PC Port

A native Windows build of Super Mario 64 DS, made from the
[sm64ds-decomp](https://github.com/tangosdev/sm64ds-decomp) decompilation. The game's own
code, compiled for the PC, with a small platform layer in place of the DS hardware.

This repo is the plain port: the base game, played the way the cartridge plays it.

## What is in here

- `port/`: the port itself (the platform layer, the build, the tools and tests).
- `decomp/`: the decompilation, as a git submodule pinned to the commit this port builds
  against (`port/DECOMP_PIN` names the same commit).

There is no game data in this repo. No ROM, no extracted files, no Nintendo art. The build
reads those from your own copy of the game.

## You need

- Your own dump of **Super Mario 64 DS, Europe** (the `.nds` file). Its SHA-1 is
  `1367529f2cb23e76ef295cb1727333ae8f0a6cd7`. Other regions do not work.
- Windows 10 or 11.
- Visual Studio 2022 (Build Tools, Community, Professional or Enterprise) with the
  "Desktop development with C++" workload, which brings the 32-bit C++ compiler, CMake and
  Ninja. The build finds it through `vswhere`, so any install folder works.
- Python 3.12 or newer on `PATH`, with `pip install ndspy pefile capstone==5.0.9`.
- Git.
- `dsd` 0.11.0 from the ds-decomp toolkit
  (https://github.com/AetiasHax/ds-decomp/releases, release `v0.11.0`), saved as
  `decomp\tools\bin\dsd.exe`. It is only used once, to unpack your cartridge.

SDL3 is in the tree (`port/third_party/SDL3`); nothing to install for it.

## Build

The full steps, tested on a fresh clone, are in
[port/docs/BUILDING.md](port/docs/BUILDING.md). In short, from a command prompt:

    git clone --recurse-submodules https://github.com/tangosdev/sm64ds-pc-port.git
    cd sm64ds-pc-port

Unpack your cartridge into the decomp, make the asset catalog and the file list (once per
checkout). Use the full path to your `.nds` file:

    python decomp\tools\unpack.py "C:\path\to\your.nds"
    decomp\tools\bin\dsd.exe rom extract --rom "C:\path\to\your.nds" --output-path decomp\extracted\dsd
    python decomp\tools\asset_catalog.py generate "C:\path\to\your.nds" --manifest build\assets\files.tsv --handles build\assets\handles.tsv --nitrofs build\assets --file-header build\generated\NitroFileId.h --handle-header build\generated\AssetHandle.h
    python port\tools\ntr_manifest.py

Then build:

    port\build-port.cmd

The game is `build\port\walk_window.exe`. Run it from the repo folder.

To check a build the way the project does, run the full test battery (it builds first):

    python port\tools\battery.py

It ends with `battery: ALL GREEN`.

## Where things live

`decomp/` is found on its own. To build against another decomp checkout, set
`SM64DS_DECOMP_ROOT` to it (or pass `-DDECOMP_ROOT=` to CMake). The build output stays in
this repo's `build/` either way.

`port/README.md` is the port's own guide: how the game boots, the save file, and the
developer switches.

## Updating the decomp

The port follows the decomp in steps. A bump moves the submodule and `port/DECOMP_PIN`
together and re-applies the port's patches to the decomp (`port/decomp_patches/`). The
battery fails when the two disagree, so after a `git pull` run
`git submodule update --init` before you build.

## License

The original work in this repo is released under the MIT License, see [LICENSE](LICENSE).
It grants no rights to any Nintendo material, which is not present here. Third-party code
under `port/third_party/` keeps its own license.
