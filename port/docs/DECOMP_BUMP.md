# Moving the port to a newer decomp

The port builds the decomp's own `src/` and `include/` files at one pinned commit. A bump moves
that pin to a newer decomp main commit and brings everything in `port/` along: the names the
port uses for decomp symbols, the paths it compiles, and the port's patches to decomp files
(`port/decomp_patches/`). This is the full procedure.

What a bump changes:

| What | Why |
|---|---|
| `decomp/` (the submodule) and `port/DECOMP_PIN` | Always together. They name the same full 40-character commit. |
| Names and paths across `port/` | The decomp renames symbols and merges files; the maps below rewrite the port's references by address. |
| `port/DECOMP_MAPS_APPLIED` | Written by the map apply. Says which maps this tree carries. |
| `port/decomp_patches/` and its `BASE` | Every patch re-cut against the new commit. `BASE` must equal `port/DECOMP_PIN` or the battery fails. |
| `port/decomp_edits.tsv` | One row per patched decomp file with its kind (`HOOK`, `SRCFIX`, ...). Rows follow added, dropped and moved patches. |
| `port/hook_baseline/` | The before side of the hook placement check, recorded fresh at every bump. |

## How often

Every two weeks. The cost grows with the gap. The first bump covered a month of decomp main
(7317 commits): 7801 renamed symbols, 2075 symbols moved into merged files, and 187 of 412
patches that the automatic merge could not place. A trial run of the same tools on 2026-11-18
against four days of main (1823 commits) gave 459 renames, 207 of 402 patches that no longer
applied exactly, and 18 that still needed a person after the automatic merge in step 6 (17
conflicts and one moved file).

## Before you start

You need a working build first: follow [BUILDING.md](BUILDING.md) to the end. On top of that:

- The GitHub CLI (`gh`), only to read the decomp's CI results in step 1.
- Git Bash (it comes with Git for Windows). Every command below is Git Bash, run from the
  repo's top folder. `build/bump/` holds the scratch files; `build/` is ignored by git.
- A clean tree on a new branch.

Then record where you start:

1. Run the battery on the current pin and note the linkage number it prints (the
   `linkage: N (x%)` line). That number is the floor for step 9; a bump must not lower it.
   It was 1224 when this repo was published.

       python port/tools/battery.py

2. Record the "before" traces now, while the old decomp is still checked out (step 10 compares
   against them). Build the tracing exe and run each input you want to compare, for example
   Bob-omb Battlefield from the level start:

       port/build-port-trace.cmd
       mkdir -p build/bump/trace-old
       SM64DS_LEVEL=6 SM64DS_WINDOW_SELFTEST=300 SM64DS_SAVE_PATH=build/bump/trace-old/bob.sav \
         SM64DS_FN_TRACE=build/bump/trace-old/bob.bin SM64DS_FN_TRACE_FROM=100 SM64DS_FN_TRACE_TO=140 \
         SM64DS_NO_FOCUS=1 SM64DS_MINIMIZED=1 SM64DS_VOLUME=0 build/port-trace/walk_window.exe
       cp build/port-trace/walk_window.map build/bump/trace-old/

   `SM64DS_FN_TRACE_FROM` and `_TO` are game frames. Pick inputs that cross a transition (a
   level load, a door, a menu change), not only steady motion: a scene where nothing moves can
   hide an update that no longer runs.

3. Set the names the commands below use:

       OLD=$(tr -d '\r\n' < port/DECOMP_PIN)
       OLDPORT=$(git rev-parse HEAD)

## Steps

1. **Pick the target.** Take the newest decomp main commit whose CI is green. A `[skip ci]`
   progress refresh on top of a green commit is fine. Read the runs with:

       gh run list --repo tangosdev/sm64ds-decomp --branch main --limit 20

   Then fetch it and name it:

       git -C decomp fetch origin main
       NEW=$(git -C decomp rev-parse <the commit you picked>)

2. **Export both decomp commits.** The map builder reads two read-only copies, each with
   `config/`, `include/`, `src/` and `tools/` (it runs the decomp's own `tools/srcpath.py`
   inside each copy, which is why `tools/` is needed):

       mkdir -p build/bump/pin build/bump/target
       git -C decomp archive $OLD config include src tools | tar -x -C build/bump/pin
       git -C decomp archive $NEW config include src tools | tar -x -C build/bump/target

3. **Build the maps.**

       python port/tools/catchup_maps.py --pin build/bump/pin --target build/bump/target \
         --port port --out build/bump/maps --git-repo decomp --pin-rev $OLD --target-rev $NEW

   It writes into `build/bump/maps/`: `renames.tsv` and `renames_kind.tsv` (every symbol whose
   name changed, keyed by module and address, never by name), `srcmoves.tsv` (where each
   symbol's source file went), `patchmap.tsv` (for every patch, whether its decomp file still
   exists or where its code went), `hand.tsv` (what a person has to judge: deleted symbols,
   ambiguous ones, patched files with no new home) and `counts.txt`. Read `hand.tsv` before
   going on. `python port/tools/catchup_maps.py --help` explains every column and tag.

   The builder does not write `target.txt`, which the apply needs. Write it by hand. `PIN`
   must be spelled exactly like `target_rev` in `port/DECOMP_MAPS_APPLIED` (11 characters
   today), or the apply refuses with "records other maps":

       printf 'PIN=%s\nTARGET=%s\n' "$(tr -d '\r' < port/DECOMP_MAPS_APPLIED | sed -n 's/^target_rev //p')" "${NEW:0:11}" > build/bump/maps/target.txt

   Keep `build/bump/maps/` until any fork that merges from this repo has taken the bump: it
   applies the same maps.

4. **Apply the maps.** Dry run first (writes only the report), then for real:

       python port/tools/catchup_maps.py apply --maps build/bump/maps --port port \
         --target build/bump/target --dry-run --report build/bump/apply.txt
       python port/tools/catchup_maps.py apply --maps build/bump/maps --port port \
         --target build/bump/target --report build/bump/apply.txt

   It rewrites every text file under `port/` except `third_party/`, `decomp_patches/` and
   `decomp_overlay_files/` (step 6 re-cuts those), in one pass, whole names only. Each swap is
   listed in `build/bump/apply.txt.changes.tsv`. It never swaps a name where the swap could
   silently point at another function; those are lines in `build/bump/apply.txt` for you to
   fix by hand:

   - `hand` with a `linkage_c_to_cxx` or `linkage_cxx_to_c` tag: the decomp moved the function
     between C and C++ names. Change the port's declaration to match the decomp header (drop or
     add the `extern "C"`), and fix any `/alternatename` line that names it.
   - `hand` with `chain`, `reused_old_name` or `reused_new_name`: the name also means another
     address now. Check which address the line means and spell that.
   - `self_alias`: the swap left `/alternatename:X=X`. Delete the line.
   - `path_into_tu`, `path_split`, `path_gone`, `path_partial`: a path to a decomp file (a
     slice row, a CMake list) whose file was merged into a bigger file, split, or deleted.
     Point it at the new file, or drop it if the new file is already compiled.

   The apply writes `port/DECOMP_MAPS_APPLIED`. A second apply of the same maps changes
   nothing.

5. **Move the pin and the submodule together.**

       git -C decomp checkout --detach $NEW
       echo $NEW > port/DECOMP_PIN

   If `tools/unpack.py` or `tools/asset_catalog.py` changed in the range, redo steps 2 to 5 of
   BUILDING.md so `extracted/` and `build/assets/` match the new tools:

       git -C decomp diff --stat $OLD $NEW -- tools/unpack.py tools/asset_catalog.py

   Commit: the submodule, `port/DECOMP_PIN`, `port/DECOMP_MAPS_APPLIED` and the renamed files,
   for example "Move the port to decomp main <NEW> and apply address-keyed renames". The
   build stays red until step 8.

6. **Re-apply the patch layer.** See which patches still apply exactly to the new decomp:

       python port/tools/decomp_overlay.py check --decomp-root decomp

   Most failures after a gap are only shifted lines: the patch layer refuses any offset or fuzz
   on purpose (`port/decomp_patches/README.md`, "The exact-apply rule"). Re-place them all with
   a three-way merge (new file, old file, old file with the patch), then cut new patches from
   the result. First the old patched files, then the merge:

       python port/tools/decomp_overlay.py apply --decomp-root build/bump/pin --out build/bump/oldpatched
       (cd port/decomp_patches && find . -name '*.patch' | sed 's|^\./||; s|\.patch$||' | sort) > build/bump/paths.txt
       mkdir -p build/bump/edited
       while read -r p; do
         if [ ! -f "build/bump/target/$p" ]; then echo "GONE $p"; continue; fi
         mkdir -p "build/bump/edited/$(dirname "$p")"
         git merge-file -p "build/bump/target/$p" "build/bump/pin/$p" "build/bump/oldpatched/$p" \
           > "build/bump/edited/$p" || echo "CONFLICT $p"
       done < build/bump/paths.txt

   Then deal with what it printed, in `build/bump/edited/`:

   - `CONFLICT`: the file has conflict markers. Keep the decomp's new code and put the port's
     edit back where the same ROM logic now lives (the old patch's hunk headers say which
     function it sat in). If the decomp now does what the edit did, drop the edit: leave the
     file exactly as the decomp has it and `cut` removes the patch.
   - `GONE`: the file no longer exists. `build/bump/maps/patchmap.tsv` says where its code went.
     Copy that new file from `build/bump/target/` into `build/bump/edited/` at its new path, put
     the edit into it, replace the old path with the new one in `build/bump/paths.txt`, and
     `git rm` the old `.patch`. Never leave the old path in the list: a listed path the decomp
     lacks is turned into a whole overlay file.
   - Files under `port/decomp_overlay_files/` (none today) are whole files the decomp does not
     have. Copy each into `build/bump/edited/` at the same path and add it to the list; if the
     decomp now has that file, `cut` turns it into a patch.

   When no markers are left, cut and check:

       grep -rl '^<<<<<<<' build/bump/edited
       python port/tools/decomp_overlay.py cut --decomp-root decomp --edited-root build/bump/edited \
         --paths build/bump/paths.txt --base $NEW
       python port/tools/decomp_overlay.py check --decomp-root decomp

   The `grep` must print nothing and `check` must exit 0. `cut` writes `BASE`; pass the full
   commit so it matches `port/DECOMP_PIN`.

   Update `port/decomp_edits.tsv` for every patch you added, moved or dropped: a new or moved
   patch gets a row under its new path with its kind (`HOOK` for a hook into game code), a
   dropped one is marked `D` with the reason in the note column. The next bump reads this
   table to know which patches are hooks.

   Commit: `port/decomp_patches/` and `port/decomp_edits.tsv`.

7. **Check where the hooks landed.** A three-way merge can apply cleanly and still put a hook
   into the wrong function. The hook check compares every `HOOK` patch before the bump with the
   same change after it, by the function it sits in (matched by address, so a renamed function
   still counts as the same). Record the new before side from the commit you started on, then
   run the check. The recorder reads both the old decomp and the old port commit out of one
   repository, so fetch the port commit into the submodule first. Old allow rows belong to the
   last bump; delete them:

       git -C decomp fetch --no-tags .. $OLDPORT
       rm port/hook_baseline/allow.tsv
       python port/tools/hook_baseline.py record --repo decomp --pin $OLD --stage0 $OLDPORT --maps build/bump/maps
       python port/tools/hook_baseline.py check

   `check` passes only with zero `MOVED` and zero `UNKNOWN` and when it covers every recorded
   `HOOK` patch; the full report is `build/hook_baseline/report.tsv`. For each hunk you moved on
   purpose (the ROM logic itself moved) or dropped (the decomp now covers it), add one row to
   `port/hook_baseline/allow.tsv`:

       <old patch path, without .patch><TAB><hunk number, or *><TAB><why, with the evidence>

   Commit `port/hook_baseline/`. The battery runs the same check on every run.

8. **Build to a clean link.**

       port/build-port.cmd

   It applies the patch layer into `build/port/decomp-overlay/`, runs the guard wave before
   configure, builds, and runs the post-link guards (`gxport_guard`, `pmf_guard`,
   `alternatename_guard`, `gxband_guard`, `tailjump_guard`, `vptr_addend_guard`; linking
   `walk_window` also runs `facecycle_guard`, `arm9_globals_guard` and `dsstate_guard`). Fix
   every red line from a guard as part of the bump. Two more checks run on request, the ABI
   checkers (each proved green on this build and red on a broken copy) and the shared-name
   alias audit:

       python port/tools/abi_prove.py
       python port/tools/alias_audit.py

   If an error makes no sense, delete `build/port` and build again before chasing it: a
   generated file left from the old pin can look current.

   The first bump also ran two comparisons that are not in this repo yet: every `/alternatename`
   target against the ROM address it named before the bump (a re-pointed alias links cleanly
   and `alternatename_guard` only catches defeated ones), and every symbol a linked decomp file
   defined against its new home. Until they are added, check each `hand` item from step 4 by
   eye, and treat any drop in the linkage number in step 9 as one of these.

9. **Battery.**

       python port/tools/battery.py --linked-floor <the number from "Before you start">

   It must end `battery: ALL GREEN`. It checks that `port/DECOMP_PIN` and `BASE` agree (a
   submodule at another commit is only a warning), runs the hook check, builds, runs every
   smoke test, every level and scene selftest and the default boot, checks the linkage number
   against the floor, then builds and starts the shipping configuration. A linkage number below
   the floor means a decomp file the port used to compile is now replaced by a host stand-in
   again: find which and fix it. Note the new number in the commit message; it is the next bump's floor.

10. **Trace replay.** Build the tracing exe again and record the same inputs, with the same
    environment, into `build/bump/trace-new/` (as in "Before you start", step 2). Compare each
    pair:

        python port/tools/tracediff.py --a build/bump/trace-old/bob.bin --a-map build/bump/trace-old/walk_window.map \
          --b build/bump/trace-new/bob.bin --b-map build/bump/trace-new/walk_window.map

    Two runs of the same exe never give identical bytes, so judge by the functions it names.
    Each difference has to trace back to a decomp change in the range that makes the port
    closer to the cartridge; anything else is a bug in the bump. Traces only compare when
    the input is identical. The scripted recorder and the pass/fail gate used at the first
    bump are not in this repo yet; until they are, this comparison is by hand.

11. **Kit smoke.** Build the shipping kit and run it the way a player does, from a clean folder
    with no repo:

        powershell -ExecutionPolicy Bypass -File port/tools/portable_kit/package_kit.ps1 \
          -Output build/kit/bump -SmokeRom "C:/path/to/your.nds"

    It ends with `kit_smoke.py`'s verdict, after opening the game window for a few seconds.
    The same check runs on its own with the line below (`--quiet` keeps the window minimized
    and muted):

        python port/tools/kit_smoke.py build/kit/bump "C:/path/to/your.nds" --quiet

12. Push the branch and open a pull request. The repo's CI (`.github/workflows/build.yml`)
    builds the link and the smoke targets without a cartridge.

## What broke at the first bump, and the fix

- **Renamed symbols.** About 7800 names changed, most from C names to C++ class methods (one
  decomp change alone moved 1517 functions from C to C++ linkage). The maps renamed the plain
  cases. The linkage flips were fixed by hand: the port's `extern "C"` declarations became C++
  declarations from the decomp headers, and `/alternatename` lines were rewritten to the new
  decorated names. Every alias was then checked to still reach the same ROM address it did
  before, because a wrong alias links cleanly (step 8 says what of that is not in this repo).
- **Moved files.** Main merged many one-function files into whole translation units and turned
  `.c` files into `.cpp`. Slice rows and CMake paths that named the old file were pointed at
  the merged file; patches whose file was gone were re-cut against the file that now holds the
  code (`patchmap.tsv`).
- **Shifted lines.** Headers gained and lost lines, so most patches no longer applied at their
  line numbers. The three-way merge re-placed them and the hook check proved each hook stayed
  in its function.
- **Patch conflicts.** Where main rewrote the lines a patch touched, the edit was put back where
  the same ROM logic now lives, keeping main's code. Edits main had made unnecessary were
  dropped, each with an allow row and its evidence.
- **Also seen.** A host copy of a global the decomp now defines itself (two storages for one
  ROM variable: the game wrote one, the port read the other). A decomp header macro that wrote
  hardware registers directly, bypassing the port's register model (every 3D model drew
  untextured). A host function seated in a ROM table with the wrong calling convention (the
  stack ended one word off). A trace difference that was a fix: the old port read a `bool`
  return as `int`, and the new tree was the one matching the cartridge. A green build from a
  stale generated file: build from a clean directory before trusting a result.
