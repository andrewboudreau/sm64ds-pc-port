# The port's decomp patch layer

The port compiles the decomp's own `src/` and `include/` files. Where the host build needs a change to
one of those files, the change lives here as a patch instead of inside the decomp tree, so the decomp
files stay exactly as they are on decomp main and a newer decomp can be pinned without a merge.

## Layout

| Path | What it holds |
|---|---|
| `port/decomp_patches/<decomp relative path>.patch` | One unified diff per decomp file (`a/` `b/` prefixes, the same shape `git diff` writes). |
| `port/decomp_patches/BASE` | One line: the decomp commit the patches were cut against. |
| `port/decomp_overlay_files/<decomp relative path>` | Whole files for paths the decomp does not have at all. |
| `.gitattributes` in both dirs | `* -text`. Patches and overlay files are byte exact; git must never convert their line endings (with `core.autocrlf=true` it would rewrite the CRLF lines inside a patch and break it). `cut` writes these files if they are missing. |

Example: the host edits to `src/engine/save/SaveData.cpp` live in
`port/decomp_patches/src/engine/save/SaveData.cpp.patch`.

## Commands

All commands are `python port/tools/decomp_overlay.py <command> ...`. `--port-dir` defaults to this
`port/` directory.

```sh
# write the overlay the build compiles from (what configure and port/build-port.cmd run)
python port/tools/decomp_overlay.py apply --decomp-root <decomp checkout> \
    --out <build dir>/decomp-overlay --include-closure include

# dry run: exit 0 when every patch applies exactly, otherwise list the failures and exit 1
python port/tools/decomp_overlay.py check --decomp-root <decomp checkout>

# regenerate patches from an edited copy of the tree
python port/tools/decomp_overlay.py cut --decomp-root <decomp checkout> --edited-root <edited tree> \
    --paths <listing> [--base <decomp commit>]
```

`apply` writes the patched copy of every patched file and a verbatim copy of every overlay file to
`<overlay dir>/<same relative path>`, then `<overlay dir>/overlay_manifest.txt` (one relative path per
line, sorted). It only ever reads the decomp root. It refuses an `--out` that is the decomp root, holds
it, or sits inside a directory its outputs mirror (`src/`, `include/`, ...), where a write could land on
a decomp file; a build dir under the checkout (`build/port/decomp-overlay`) is fine. Files that already
hold the right bytes are not rewritten, so an incremental build does not see them change. Files a
previous `apply` wrote (found through the previous manifest) that are no longer in the layer are
removed; nothing else in the overlay dir is touched.

`--include-closure <dir>` (repeatable; the build passes `include`) also copies in, unchanged, every
decomp file under `<dir>` that must sit next to an overlay file for the compiler to find the right one.
MSVC looks for `#include "x.h"` in the including file's own folder before any `-I` path, so putting
`<overlay>/include` first on the include path is not enough by itself: a raw header that includes a
patched sibling would find the raw sibling. So, to a fixpoint, a decomp header under `<dir>` whose quoted
include names an overlay file is copied in (the includer rule), and an overlay file in a subfolder of
`<dir>` gets the raw siblings it includes (the includee rule). Pulled files are listed in the manifest like
every other output. With no patches nothing is pulled; one patch to a widely included header (for example
`types.h`) pulls hundreds of headers, which is expected.

### Where the build puts the overlay

The overlay always lives in the build dir it belongs to: `<build dir>/decomp-overlay/`, with its
manifest at `<build dir>/decomp-overlay/overlay_manifest.txt`. Each build dir (`build/port`,
`build/port-kit`, `build/port-trace`, a lane's private dir) has its own, written by its own configure
(`port/cmake/decomp_overlay.cmake`). Never point two build dirs at one overlay and never write one into
the source tree. `port/build-port.cmd` also runs the same `apply` into `build/port/decomp-overlay` before
its pre-configure guard wave, so the guards see the overlay on a fresh build dir too; configure's own
`apply` after it then rewrites nothing.

### How tools read decomp files: one lookup

A port tool that reads decomp source must read the text the build compiles, not the raw decomp file, or
it checks the wrong code without any error. Every such tool goes through
`port/tools/decomp_overlay_paths.py`:

```python
import decomp_overlay_paths
view = decomp_overlay_paths.DecompView(decomp_root, build_dir)   # build_dir None: <root>/build/port
view.path("src/cstd.cpp")          # overlay copy if the manifest lists it, else the raw file
view.walk("src", (".c", ".cpp"))   # [(rel, path)] for a tree, overlay copies and overlay-only files in
view.listdir("src")                # the same, one level only
```

The rules: a path the manifest lists reads from the overlay; a path it does not list reads from the
decomp; with no manifest at all the tool reads raw files, unless the layer (`port/decomp_patches/*.patch`
or `port/decomp_overlay_files/*`) holds anything, in which case `DecompView` raises `OverlayMissing`
instead of answering, because raw files are then the wrong text. Each tool takes `--build-dir` to pick
the build whose overlay it reads. Tools that classify object or source PATHS (vptr_addend_guard,
tailjump_guard) treat `<build dir>/decomp-overlay/<rel>` as the decomp file `<rel>`: an overlay copy is
the decomp TU, in scope, never a host copy.

Tools that read through the lookup: abi_prove, alias_audit, aritycheck (and run_checks, which runs it),
arm9_globals_guard, closestplayer_guard, dtorfwd, hostgen_portsync3 (follows hostgen's `--decomp`
root, which `port/cmake/hostgen_overlay.cmake` points at the overlay), inferred_stub_guard, ovdata
(`--cross` reads the build dir off its `<build>/host-src/ov_cross.c` output path), stategen,
tu_props_audit, vtablerows. A new tool that reads decomp source uses the lookup too.

`cut` reads a listing of decomp relative paths (one per line, blank lines and `#` comments ignored). For
each path:

- in both trees and different: the patch is (re)written;
- in both trees and identical: its patch is removed;
- missing from the decomp: the edited file goes to `port/decomp_overlay_files/` (and any old patch is
  removed);
- missing from the edited tree: an error (deleting a decomp file is not something the layer does).

Every patch `cut` writes is applied back in memory before it is kept, so a cut patch always round-trips.

## The exact-apply rule

A patch applies only if every hunk matches the decomp file byte for byte at exactly the line number the
hunk names. A hunk that would fit somewhere else (an offset) or only with relaxed context (fuzz) is a
failure, never a warning: `apply` and `check` name the file, say which hunk, and report the offset it
would have needed when one exists. Nothing is written when any patch fails. The reason is that the decomp
reshuffles files; a hook that slides into a neighbouring function and still "applies" is the silent
failure this layer exists to prevent.

Every hunk also needs 3 lines of context before its first change and 3 after its last, unless it starts
at the first line of the file or ends at the last one. A hunk with less context (a hand-written patch, or
`git diff -U0`) checks nothing around it and would land wherever its line number points, so it is
refused. `cut` always writes full context. A new-file patch (`--- /dev/null`) is refused too: files the
decomp lacks go in `decomp_overlay_files/`.

An overlay file whose path has since appeared in the decomp is also a failure: it has to be re-cut as a
patch against the decomp's version.

Paths are compared with their exact letter case, on Windows as on Linux. A patch named
`include/bar.h.patch` does not apply to the decomp's `include/Bar.h`, an overlay file may not differ only
in case from a decomp file, two layer paths may not differ only in case, and `cut` refuses a listed path
whose case differs from the file on disk.

`apply` writes in a crash-safe order: first a manifest listing both the previous and the new outputs, then
it removes stale outputs, then it writes the new ones, then the final manifest. A case-only rename
(`Foo.h` to `foo.h`) therefore deletes the old name before writing the new one.

## Bytes

Files are handled as bytes. A missing final newline (`\ No newline at end of file`), lone CRs and
non-UTF-8 bytes (the decomp has Shift-JIS comments) survive `cut` and `apply` unchanged. The patches are
ordinary unified diffs; `git apply` accepts them on an LF checkout. The tool applies them itself so it
can refuse offsets and fuzz.

## Line endings

One set of patches serves a CRLF checkout (Windows with `core.autocrlf=true`; `git archive` on such a
machine gives CRLF too) and an LF checkout (Linux CI, a clone with autocrlf off). The rule follows what
git itself stores:

- **Stored form.** `cut` turns a file whose line endings are uniformly CRLF into LF before diffing, on
  both sides. Patches and overlay files are therefore stored in LF, the form git keeps in its index, and
  a cut on a CRLF checkout writes the same patch as a cut on an LF checkout.
- **Applying.** If the decomp file is uniformly CRLF, `apply` turns it into LF, applies the patch exactly
  (still zero offset, zero fuzz), and writes the result back with CRLF. A uniformly LF decomp file gives
  an LF result.
- **Mixed line endings are never normalized.** A file with both CRLF and bare LF lines is stored that way
  in git and checked out unchanged on every machine, so it is patched and cut byte for byte. The same
  holds when an edit leaves a file mixed: the edited bytes are kept as they are. The decomp has such
  files: 0 at 03a6d6aea41 (the first `BASE`), 6 on main as of 2026-11-14 (`include/dScMgCup_c.h`,
  `include/dScMgMCarlo2_c.h`, `include/dScMgMemory2_c.h`, `include/dScMgTrampoline2_c.h`,
  `include/dScMgTrampoline_c.h`, `src/_ZN10dBgW_KcMbgC1Ev.cpp`). The port at game 1.0.7 also carries one
  edit that left `include/dScMgMCarlo2_c.h` mixed. Count them with
  `git ls-files --eol src include | grep -c i/mixed`.
- **Tree convention.** Overlay files, and patched decomp files that are mixed or have no newline at all,
  are written in the convention of the decomp checkout. `--eol auto` (the default) takes the majority of
  the patched decomp files that are uniformly CRLF or LF, and if there are none, of up to 64 source files
  sampled from `include/` and `src/`. `--eol lf` or `--eol crlf` overrides the guess.

## Bumping the decomp

1. Check out the new decomp commit.
2. `check --decomp-root <new checkout>`. Every patch that still applies exactly is fine as it is.
3. For each failure, port the edit by hand onto the new decomp file in an edited tree (the old patch
   and its hunk headers say what the edit was and where it sat), checking that each hook lands in the
   same function it hooked before.
4. `cut --decomp-root <new checkout> --edited-root <edited tree> --paths <listing of every patched or
   overlay path> --base <new commit>`. That rewrites the patches, removes ones that became unnecessary,
   moves overlay files that the decomp now has into patches, and updates `BASE`.
5. `check` again: it must exit 0.
