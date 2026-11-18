# Credits

The PC port is built on the [sm64ds-decomp](https://github.com/tangosdev/sm64ds-decomp)
decompilation. Every function the port runs was matched by someone there, so everyone in the
decomp's [CREDITS.md](https://github.com/tangosdev/sm64ds-decomp/blob/main/CREDITS.md) is part
of this port too, along with the SM64DS community reference work that file credits.

This repo starts from one import commit, so its git history does not show who wrote what
before the move. This file does.

## Project lead
- **Tango** (https://github.com/tangosdev) - the PC port: the platform layer, the build, the
  tools and the tests.

## Port contributions
- **Andrew Boudreau** (https://github.com/andrewboudreau) - kept the port building through the
  decomp's big file and class migrations (the heap family, the actor file names, the folded
  translation units), carrying each rename into the port's slices and host code; repaired the
  port's smoke build after the constructor migration (decomp PRs #1713, #1748); and set up the
  port build check on the decomp's main branch (decomp PRs #2372, #2373).
- **lunavyqo** (https://github.com/lunavyqo) - carried the port along with the decomp's class
  translation-unit cleanup: calling the converted methods by name instead of their mangled
  spellings, and taking the fakematch `extern "C"` declarations out of the arm9 code.
- **ruspecial** (https://github.com/ruspecial) - host copies for the two Bowser ov060 holes
  (decomp PR #1505), and the port waves that mounted the koopa1 and koopa3 boss arenas
  (levels 36 and 40) with the ov044 cast, brought Big Boo's Haunt to zero skips with the Mr. I
  family, laid out the stylus record, and measured the stage storage.
- **mitchellcairns** (https://github.com/mitchellcairns) - moved the GX namespace out of C mode
  across the port (phase 1 of decomp #821).
- **Alberto12345678999** (https://github.com/Alberto12345678999) - fixed code-block
  highlighting across the port's docs (decomp PR #1658).

## Testing and bug reports
The players who found bugs and sent reports, as credited in the game's changelog:
Thunder, Mauricio, ExcellentGamer, eljony, KDT23, Blueji, smbfan, DieMitchell, felonthoughts,
ThePoopster, KDokie, BegunKnight027, ItsGameBoy, Astr0th3man, Erableto, Davrew, weegee,
retroeddy, mintp0p, Lilith, Zenn, Béla, mbwolt, AshurathDR, BlazeTendo and darkblood159.

## Third-party code
- **SDL3** (https://github.com/libsdl-org/SDL), by Sam Lantinga and the SDL contributors,
  zlib license: `port/third_party/SDL3`.
- **stb_image** and **stb_image_write** (https://github.com/nothings/stb), by Sean Barrett,
  public domain or MIT: `port/third_party/stb`.

## Nintendo
Super Mario 64 DS is by Nintendo EAD. This repo holds none of Nintendo's code, art or data; the
build reads them from your own copy of the game.
