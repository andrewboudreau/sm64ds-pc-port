/* hal/extracted_root.h -- where a run finds the unpacked cartridge (extracted/).

   Every seam that opens a cartridge file reads it under <x>/extracted/dsd/...,
   and every one of them used to take <x> to be the asset root
   (SM64DS_ASSET_ROOT, else PORT_REPO_ROOT, the port's own tree). That holds
   while port/ sits inside a decomp checkout, because the port's tree and the
   decomp are then one folder. In the public repo they are two: port/ sits
   beside a decomp/ submodule, build/assets is in the port's tree and the
   ROM dump is in decomp/extracted. port/tools/decomp_root.py has the same rule
   for the tools; this is the game's half of it.

   port_extracted_dir(out, n, asset_root) writes the extracted/ folder a run
   with that asset root reads, first match wins:
     1. <asset root>/extracted, when it is a folder. The in-tree layout and
        every packaged kit (the kit's extractor writes it there). Nothing
        changes for them.
     2. <PORT_DECOMP_DIR>/extracted, when it is a folder: the decomp the build
        compiled (CMake's DECOMP_ROOT). Developer builds only; a ROM-clean build
        embeds no build-machine path, so PORT_DECOMP_DIR is "" there.
     3. <asset root>/decomp/extracted, when it is a folder: the public layout
        with no path baked in (what the ROM-clean exe the battery runs from a
        checkout needs).
     4. <asset root>/extracted, so a missing dump is reported where it always
        was.

   PORT_DECOMP_DIR is a source-level define, given only to the files that read
   the ROM dump (port/CMakeLists.txt, PORT_DECOMP_DIR_READERS), the way
   PORT_REPO_ROOT is. A file that includes this without it skips step 2. */
#ifndef PORT_HAL_EXTRACTED_ROOT_H
#define PORT_HAL_EXTRACTED_ROOT_H

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

static inline int port_extracted_isdir(const char *dir)
{
    struct stat st;
    return stat(dir, &st) == 0 && (st.st_mode & S_IFDIR) != 0;
}

static inline const char *port_extracted_dir(char *out, size_t n, const char *asset_root)
{
    snprintf(out, n, "%s/extracted", asset_root);
    if (port_extracted_isdir(out))
        return out;
#if defined(PORT_DECOMP_DIR) && !defined(PORT_ROM_CLEAN)
    if (PORT_DECOMP_DIR[0]) {
        snprintf(out, n, "%s/extracted", PORT_DECOMP_DIR);
        if (port_extracted_isdir(out))
            return out;
    }
#endif
    snprintf(out, n, "%s/decomp/extracted", asset_root);
    if (port_extracted_isdir(out))
        return out;
    snprintf(out, n, "%s/extracted", asset_root);
    return out;
}

#endif /* PORT_HAL_EXTRACTED_ROOT_H */
