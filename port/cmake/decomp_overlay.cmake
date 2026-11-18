# ---- THE DECOMP PATCH OVERLAY (run catchup1, Task 1.3) -----------------------
#
# The port compiles the decomp's own src/ and include/ files. Where the host
# needs a change to one of them, the change lives in port/decomp_patches/ (one
# patch per file) or port/decomp_overlay_files/ (whole files the decomp does not
# have); port/decomp_patches/README.md has the layer's rules. This file makes the
# build READ the decomp through that layer:
#
#   - configure runs `decomp_overlay.py apply` into ${PORT_OVERLAY_DIR} and
#     fails if any patch does not apply exactly;
#   - every target source that names a decomp file with an overlay copy is
#     compiled from the copy (port_overlay_reroute, run before the SLICE SHARE
#     split and again before the PORTSYNC2 reroute, then checked at the end);
#   - ${PORT_OVERLAY_DIR}/include goes immediately before the decomp include
#     dir on every target (port_overlay_include_dirs, end of file);
#   - every hostgen run goes through port/cmake/hostgen_overlay.cmake, which
#     hands hostgen the overlay copy when there is one (so a whole-TU hostgen
#     substitution of a patched TU reads the patched text, never the raw file);
#   - configure-time readers of decomp content in CMakeLists.txt ask
#     port_decomp_file() for the path to read.
#
# With no patches the overlay is empty, every source keeps its raw path and the
# only change to a compile line is one more (empty) -I directory.
#
# Readers that live in tools this file cannot reach (they derive the decomp
# root from their own location) are named in port_overlay_refuse_raw_reader()
# calls: configure fails if the layer ever patches a file one of them reads,
# rather than letting the tool read the raw file while the build compiles the
# patched one.

# DECOMP_ROOT is defined at the top of CMakeLists.txt (run catchup1, Task 2.1).
get_filename_component(PORT_DECOMP_ROOT "${DECOMP_ROOT}" ABSOLUTE)
set(PORT_OVERLAY_DIR "${CMAKE_BINARY_DIR}/decomp-overlay")
set(PORT_OVERLAY_TOOL "${CMAKE_CURRENT_SOURCE_DIR}/tools/decomp_overlay.py")
set(PORT_OVERLAY_MANIFEST "${PORT_OVERLAY_DIR}/overlay_manifest.txt")
set(PORT_OVERLAY_PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/decomp_patches")
set(PORT_OVERLAY_FILES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/decomp_overlay_files")

# An overlay dir with no manifest was not written by apply (or apply died before
# its first write): nothing in it can be trusted, and apply only cleans what a
# manifest names, so start it again from nothing.
if(EXISTS "${PORT_OVERLAY_DIR}" AND NOT EXISTS "${PORT_OVERLAY_MANIFEST}")
    message(STATUS "DECOMP OVERLAY: ${PORT_OVERLAY_DIR} has no manifest, wiping it")
    file(REMOVE_RECURSE "${PORT_OVERLAY_DIR}")
endif()
set(_pov_prev "")
if(EXISTS "${PORT_OVERLAY_MANIFEST}")
    file(STRINGS "${PORT_OVERLAY_MANIFEST}" _pov_prev)
endif()

execute_process(
    COMMAND python "${PORT_OVERLAY_TOOL}" apply
            --decomp-root "${PORT_DECOMP_ROOT}"
            --out "${PORT_OVERLAY_DIR}"
            --include-closure include
    RESULT_VARIABLE _pov_rc
    OUTPUT_VARIABLE _pov_out
    ERROR_VARIABLE _pov_err
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _pov_rc EQUAL 0)
    message(FATAL_ERROR "DECOMP OVERLAY: decomp_overlay.py apply failed (${_pov_rc}); "
            "a patch that does not apply exactly is a stop, never a skip.\n"
            "${_pov_out}\n${_pov_err}")
endif()
message(STATUS "DECOMP OVERLAY: ${_pov_out}")
if(NOT EXISTS "${PORT_OVERLAY_MANIFEST}")
    message(FATAL_ERROR "DECOMP OVERLAY: apply exited 0 but wrote no ${PORT_OVERLAY_MANIFEST}")
endif()
# the -I entry must name a real directory even while the overlay has no headers
file(MAKE_DIRECTORY "${PORT_OVERLAY_DIR}/include")

file(STRINGS "${PORT_OVERLAY_MANIFEST}" PORT_OVERLAY_FILES)
foreach(_pov_rel IN LISTS PORT_OVERLAY_FILES)
    set("PORT_OVERLAY_HAS_${_pov_rel}" 1)
    # Windows opens a path without regard to letter case: the end-of-file check
    # compares on this folded key, so a source spelled ../src/Main.c is caught too
    string(TOLOWER "${_pov_rel}" _pov_fold)
    set("PORT_OVERLAY_FOLD_${_pov_fold}" "${_pov_rel}")
    # the spellings a target source takes for a decomp file in this file
    set("PORT_OVERLAY_KEY_${PORT_DECOMP_ROOT}/${_pov_rel}" "${_pov_rel}")
    set("PORT_OVERLAY_KEY_${DECOMP_ROOT}/${_pov_rel}" "${_pov_rel}")
    set("PORT_OVERLAY_KEY_../${_pov_rel}" "${_pov_rel}")
endforeach()
list(LENGTH PORT_OVERLAY_FILES PORT_OVERLAY_COUNT)

# A file under the overlay that apply did not write would shadow the decomp file
# of the same name (a header through -I, a source through a sibling include) and
# nothing would say so. apply only ever writes what its manifest lists.
file(GLOB_RECURSE _pov_on_disk LIST_DIRECTORIES false RELATIVE "${PORT_OVERLAY_DIR}"
     "${PORT_OVERLAY_DIR}/*")
set(_pov_stray "")
foreach(_pov_f IN LISTS _pov_on_disk)
    if(_pov_f STREQUAL "overlay_manifest.txt")
        continue()
    endif()
    if(NOT DEFINED "PORT_OVERLAY_HAS_${_pov_f}")
        list(APPEND _pov_stray "${_pov_f}")
    endif()
endforeach()
if(_pov_stray)
    message(FATAL_ERROR "DECOMP OVERLAY: ${PORT_OVERLAY_DIR} holds file(s) its manifest does "
            "not list, which would shadow the decomp's own: ${_pov_stray}. Delete the overlay "
            "dir (configure rebuilds it) and find what wrote them.")
endif()

# Configure inputs: the tool, every layer file and every layer DIRECTORY (a
# directory's timestamp moves when a file is added to or removed from it, which
# is how a new patch reaches RERUN_CMAKE without file(GLOB CONFIGURE_DEPENDS);
# see the CONFIGURE DEPS block at the top of CMakeLists.txt for why that is not
# used). port_configure_input() skips a path that does not exist, so the first
# decomp_overlay_files/ dir ever created needs one manual configure.
port_configure_input("${PORT_OVERLAY_TOOL}")
foreach(_pov_layer_dir "${PORT_OVERLAY_PATCHES}" "${PORT_OVERLAY_FILES_DIR}")
    if(EXISTS "${_pov_layer_dir}")
        port_configure_input("${_pov_layer_dir}")
        file(GLOB_RECURSE _pov_layer LIST_DIRECTORIES true "${_pov_layer_dir}/*")
        foreach(_pov_one IN LISTS _pov_layer)
            port_configure_input("${_pov_one}")
        endforeach()
    endif()
endforeach()
# ...and the decomp side of every overlay copy: a patched file, or a file the
# include closure copied, that changes in the decomp has to be applied again.
# While the overlay holds anything at all, every file and directory under
# include/ is an input too, because any of them can join or leave the include
# closure.
if(PORT_OVERLAY_COUNT GREATER 0)
    foreach(_pov_rel IN LISTS PORT_OVERLAY_FILES)
        if(EXISTS "${PORT_DECOMP_ROOT}/${_pov_rel}")
            port_configure_input("${PORT_DECOMP_ROOT}/${_pov_rel}")
        endif()
    endforeach()
    port_configure_input("${PORT_DECOMP_ROOT}/include")
    file(GLOB_RECURSE _pov_inc LIST_DIRECTORIES true "${PORT_DECOMP_ROOT}/include/*")
    foreach(_pov_one IN LISTS _pov_inc)
        port_configure_input("${_pov_one}")
    endforeach()
endif()

# Hostgen copies (build/<dir>/host-src/<src path>.cpp) are generated from the
# overlay copy when there is one (hostgen_overlay.cmake), but most hostgen rules
# in CMakeLists.txt name the RAW file in DEPENDS. So a generated copy older than
# its overlay copy, or made from an overlay copy that has since left the layer,
# is deleted here and ninja makes it again from the right input.
set(_pov_hg_seen "")
foreach(_pov_rel IN LISTS PORT_OVERLAY_FILES _pov_prev)
    if(NOT _pov_rel MATCHES "^src/.*[.](c|cpp)$" OR "${_pov_rel}" IN_LIST _pov_hg_seen)
        continue()
    endif()
    list(APPEND _pov_hg_seen "${_pov_rel}")
    string(REGEX REPLACE "[.](c|cpp)$" ".cpp" _pov_gen "${CMAKE_BINARY_DIR}/host-src/${_pov_rel}")
    if(NOT EXISTS "${_pov_gen}")
        continue()
    endif()
    if(DEFINED "PORT_OVERLAY_HAS_${_pov_rel}")
        if("${PORT_OVERLAY_DIR}/${_pov_rel}" IS_NEWER_THAN "${_pov_gen}")
            message(STATUS "DECOMP OVERLAY: ${_pov_rel} changed, regenerating its hostgen copy")
            file(REMOVE "${_pov_gen}")
        endif()
    else()
        message(STATUS "DECOMP OVERLAY: ${_pov_rel} left the overlay, regenerating its hostgen copy")
        file(REMOVE "${_pov_gen}")
    endif()
endforeach()

message(STATUS "DECOMP OVERLAY: ${PORT_OVERLAY_COUNT} decomp file(s) read from ${PORT_OVERLAY_DIR}")

# Every hostgen rule runs through the overlay-aware wrapper. Spelled as a list
# so `COMMAND ${PORT_HOSTGEN} <hostgen args>` expands into separate arguments.
set(PORT_HOSTGEN
    "${CMAKE_COMMAND}"
    "-DPORT_OVERLAY_DIR=${PORT_OVERLAY_DIR}"
    "-DPORT_DECOMP_ROOT=${PORT_DECOMP_ROOT}"
    "-DPORT_HOSTGEN_PY=${CMAKE_CURRENT_SOURCE_DIR}/tools/hostgen.py"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/hostgen_overlay.cmake" --)
port_configure_input("${CMAKE_CURRENT_SOURCE_DIR}/cmake/hostgen_overlay.cmake")

# No hostgen rule may bypass the wrapper: one that did would make its copy from
# the raw decomp file whether or not the overlay has a patched one.
# Any spelling counts: python, python3, py, "${PORT_PYTHON}", quoted or not, with
# the script on the COMMAND line or on the line after it (the shape every rule
# here uses). Comment lines are dropped first, and the text is lower-cased
# because CMake regexes have no case-insensitive flag.
file(READ "${CMAKE_CURRENT_LIST_DIR}/../CMakeLists.txt" _pov_self)
string(TOLOWER "${_pov_self}" _pov_self)
string(REGEX REPLACE "\n[ \t]*#[^\n]*" "\n" _pov_self "${_pov_self}")
string(REGEX MATCHALL "command[ \t]+[^\n]*hostgen[.]py" _pov_bypass1 "${_pov_self}")
string(REGEX MATCHALL "command[ \t]+[^\n]*\n[ \t]*\"?[^\n\" \t]*hostgen[.]py" _pov_bypass2
       "${_pov_self}")
unset(_pov_self)
set(_pov_bypass ${_pov_bypass1} ${_pov_bypass2})
if(_pov_bypass)
    list(LENGTH _pov_bypass _pov_bypass_n)
    message(FATAL_ERROR "DECOMP OVERLAY: ${_pov_bypass_n} hostgen rule(s) in CMakeLists.txt call "
            "tools/hostgen.py directly. Spell the command `COMMAND \${PORT_HOSTGEN} <args>` so "
            "hostgen reads the overlay copy of a patched source.")
endif()

# port_decomp_file(<out var> <decomp relative path>): the file to READ for that
# decomp path -- the overlay copy when the layer has one, else the decomp's.
function(port_decomp_file _pdf_out _pdf_rel)
    if(DEFINED "PORT_OVERLAY_HAS_${_pdf_rel}")
        set(${_pdf_out} "${PORT_OVERLAY_DIR}/${_pdf_rel}" PARENT_SCOPE)
    elseif(NOT PORT_ROW_PREFIX STREQUAL "" AND "${_pdf_rel}" MATCHES "^port/")
        # a row naming the port's own file, under an external decomp root
        set(${_pdf_out} "${PORT_TREE_DIR}/${_pdf_rel}" PARENT_SCOPE)
    else()
        set(${_pdf_out} "${PORT_DECOMP_ROOT}/${_pdf_rel}" PARENT_SCOPE)
    endif()
endfunction()

# port_overlay_refuse_raw_reader(<reader> <decomp relative path or absolute
# decomp path>...): <reader> reads those files from the raw decomp tree and
# cannot be pointed at the overlay from here, so the layer may not patch them.
function(port_overlay_refuse_raw_reader _porr_who)
    foreach(_porr_p IN LISTS ARGN)
        if(IS_ABSOLUTE "${_porr_p}")
            file(RELATIVE_PATH _porr_p "${PORT_DECOMP_ROOT}" "${_porr_p}")
        endif()
        if(DEFINED "PORT_OVERLAY_HAS_${_porr_p}")
            message(FATAL_ERROR "DECOMP OVERLAY: ${_porr_p} has an overlay copy, but ${_porr_who} "
                    "reads the raw decomp file, so the build would compile one text and that "
                    "tool would read another. Route the tool through the overlay first.")
        endif()
    endforeach()
endfunction()

# hostgen_portsync3.py reads src/cstd.cpp under hostgen's --decomp root, which
# port/cmake/hostgen_overlay.cmake points at the overlay; no reader is refused
# today. Keep the function for any new raw reader.

# port_overlay_reroute(<label>): every target source naming a decomp file that
# has an overlay copy is replaced by the copy. Idempotent; cheap when the
# overlay is empty. The source-file properties set on the raw path are copied
# onto the copy by port_overlay_finish() at the end of CMakeLists.txt, after the
# last set_source_files_properties() call.
function(port_overlay_reroute _por_label)
    if(PORT_OVERLAY_COUNT EQUAL 0)
        return()
    endif()
    get_property(_por_targets DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(_por_done GLOBAL PROPERTY PORT_OVERLAY_REROUTED)
    set(_por_hits 0)
    foreach(_por_t IN LISTS _por_targets)
        get_target_property(_por_type ${_por_t} TYPE)
        if(_por_type STREQUAL "INTERFACE_LIBRARY" OR _por_type STREQUAL "UTILITY")
            continue()
        endif()
        get_target_property(_por_srcs ${_por_t} SOURCES)
        if(NOT _por_srcs)
            continue()
        endif()
        set(_por_new "")
        set(_por_changed FALSE)
        foreach(_por_s IN LISTS _por_srcs)
            if(DEFINED "PORT_OVERLAY_KEY_${_por_s}")
                set(_por_rel "${PORT_OVERLAY_KEY_${_por_s}}")
                list(APPEND _por_new "${PORT_OVERLAY_DIR}/${_por_rel}")
                if(NOT "${_por_rel}" IN_LIST _por_done)
                    list(APPEND _por_done "${_por_rel}")
                endif()
                set(_por_changed TRUE)
                math(EXPR _por_hits "${_por_hits} + 1")
            else()
                list(APPEND _por_new "${_por_s}")
            endif()
        endforeach()
        if(_por_changed)
            set_property(TARGET ${_por_t} PROPERTY SOURCES ${_por_new})
        endif()
    endforeach()
    set_property(GLOBAL PROPERTY PORT_OVERLAY_REROUTED "${_por_done}")
    message(STATUS "DECOMP OVERLAY (${_por_label}): ${_por_hits} target source(s) moved to "
                   "their overlay copies")
endfunction()

# port_overlay_finish(): last thing in CMakeLists.txt.
#   1. no target compiles a raw decomp file that has an overlay copy, however
#      the path is spelled (checked on the normalised absolute path);
#   2. the raw path's source-file properties are copied onto the overlay copy
#      (the per-TU defines and options this file sets by raw path are part of
#      what makes a TU build right; a dropped one builds without error);
#   3. ${PORT_OVERLAY_DIR}/include goes immediately before the decomp include
#      dir on every target that has it, and nothing names the decomp include
#      dir without it.
set(PORT_OVERLAY_SOURCE_PROPS
    COMPILE_DEFINITIONS COMPILE_OPTIONS COMPILE_FLAGS INCLUDE_DIRECTORIES LANGUAGE
    OBJECT_DEPENDS OBJECT_OUTPUTS SKIP_PRECOMPILE_HEADERS HEADER_FILE_ONLY
    SKIP_UNITY_BUILD_INCLUSION UNITY_GROUP)
# port_tree_reroute(): under an external decomp root (PORT_ROW_PREFIX set), a
# target source or include dir spelled under the decomp root's port/ or build/
# (a "port/..." row read as "${DECOMP_ROOT}/${row}") is the port's own file:
# move it to the port tree and carry its source-file properties with it, the
# same way the overlay reroute carries a patched source's. Nothing to do in the
# in-tree layout, where the two roots are one directory.
function(port_tree_reroute)
    if(PORT_ROW_PREFIX STREQUAL "")
        return()
    endif()
    get_property(_ptr_targets DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
    string(TOLOWER "${PORT_DECOMP_ROOT}/" _ptr_fold)
    string(LENGTH "${_ptr_fold}" _ptr_cut)
    set(_ptr_n 0)
    set(_ptr_inc_n 0)
    foreach(_ptr_t IN LISTS _ptr_targets)
        get_target_property(_ptr_type ${_ptr_t} TYPE)
        if(NOT _ptr_type STREQUAL "INTERFACE_LIBRARY" AND NOT _ptr_type STREQUAL "UTILITY")
            get_target_property(_ptr_srcs ${_ptr_t} SOURCES)
            if(_ptr_srcs)
                set(_ptr_new "")
                set(_ptr_changed FALSE)
                foreach(_ptr_s IN LISTS _ptr_srcs)
                    set(_ptr_to "${_ptr_s}")
                    if(NOT _ptr_s MATCHES "\\$<")
                        get_filename_component(_ptr_abs "${_ptr_s}" ABSOLUTE
                                               BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
                        string(TOLOWER "${_ptr_abs}" _ptr_low)
                        string(FIND "${_ptr_low}" "${_ptr_fold}" _ptr_at)
                        if(_ptr_at EQUAL 0)
                            string(SUBSTRING "${_ptr_abs}" ${_ptr_cut} -1 _ptr_rel)
                            if(_ptr_rel MATCHES "^(port|build)/")
                                set(_ptr_to "${PORT_TREE_DIR}/${_ptr_rel}")
                                foreach(_ptr_prop IN LISTS PORT_OVERLAY_SOURCE_PROPS)
                                    get_source_file_property(_ptr_v "${_ptr_abs}" ${_ptr_prop})
                                    if(NOT _ptr_v STREQUAL "NOTFOUND")
                                        set_source_files_properties("${_ptr_to}"
                                            PROPERTIES ${_ptr_prop} "${_ptr_v}")
                                    endif()
                                endforeach()
                                set(_ptr_changed TRUE)
                                math(EXPR _ptr_n "${_ptr_n} + 1")
                            endif()
                        endif()
                    endif()
                    list(APPEND _ptr_new "${_ptr_to}")
                endforeach()
                if(_ptr_changed)
                    list(REMOVE_DUPLICATES _ptr_new)
                    set_property(TARGET ${_ptr_t} PROPERTY SOURCES ${_ptr_new})
                endif()
            endif()
        endif()
        foreach(_ptr_prop INCLUDE_DIRECTORIES INTERFACE_INCLUDE_DIRECTORIES)
            get_target_property(_ptr_dirs ${_ptr_t} ${_ptr_prop})
            if(NOT _ptr_dirs)
                continue()
            endif()
            set(_ptr_new "")
            set(_ptr_changed FALSE)
            foreach(_ptr_d IN LISTS _ptr_dirs)
                set(_ptr_to "${_ptr_d}")
                if(NOT _ptr_d MATCHES "^\\$<")
                    get_filename_component(_ptr_abs "${_ptr_d}" ABSOLUTE
                                           BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
                    string(TOLOWER "${_ptr_abs}/" _ptr_low)
                    string(FIND "${_ptr_low}" "${_ptr_fold}" _ptr_at)
                    if(_ptr_at EQUAL 0)
                        string(SUBSTRING "${_ptr_abs}/" ${_ptr_cut} -1 _ptr_rel)
                        if(_ptr_rel MATCHES "^(port|build)/")
                            set(_ptr_to "${PORT_TREE_DIR}/${_ptr_rel}")
                            string(REGEX REPLACE "/$" "" _ptr_to "${_ptr_to}")
                            set(_ptr_changed TRUE)
                            math(EXPR _ptr_inc_n "${_ptr_inc_n} + 1")
                        endif()
                    endif()
                endif()
                list(APPEND _ptr_new "${_ptr_to}")
            endforeach()
            if(_ptr_changed)
                set_property(TARGET ${_ptr_t} PROPERTY ${_ptr_prop} "${_ptr_new}")
            endif()
        endforeach()
    endforeach()
    message(STATUS "DECOMP_ROOT: ${_ptr_n} target source(s) and ${_ptr_inc_n} include dir(s) "
                   "spelled under the decomp root's port/ or build/ moved to the port tree")
endfunction()

function(port_overlay_finish)
    port_tree_reroute()
    get_property(_pof_targets DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
    set(_pof_inc "${PORT_DECOMP_ROOT}/include")
    set(_pof_ovl_inc "${PORT_OVERLAY_DIR}/include")
    set(_pof_raw "")
    set(_pof_gen "")
    string(REPLACE "\\" "/" _pof_root_fold "${PORT_DECOMP_ROOT}")
    string(TOLOWER "${_pof_root_fold}" _pof_root_fold)
    set(_pof_inc_n 0)
    foreach(_pof_t IN LISTS _pof_targets)
        get_target_property(_pof_type ${_pof_t} TYPE)
        if(PORT_OVERLAY_COUNT GREATER 0 AND NOT _pof_type STREQUAL "INTERFACE_LIBRARY"
           AND NOT _pof_type STREQUAL "UTILITY")
            get_target_property(_pof_srcs ${_pof_t} SOURCES)
            if(_pof_srcs)
                foreach(_pof_s IN LISTS _pof_srcs)
                    if(_pof_s MATCHES "\\$<")
                        # A generator expression is only resolved at generate time,
                        # after this check, so it cannot be rerouted or proven safe
                        # here. One that names decomp src/ or include/ text is refused.
                        string(TOLOWER "${_pof_s}" _pof_gx)
                        string(REPLACE "\\" "/" _pof_gx "${_pof_gx}")
                        string(FIND "${_pof_gx}" "${_pof_root_fold}/src/" _pof_g1)
                        string(FIND "${_pof_gx}" "${_pof_root_fold}/include/" _pof_g2)
                        if(NOT _pof_g1 EQUAL -1 OR NOT _pof_g2 EQUAL -1
                           OR _pof_gx MATCHES "(^|[^a-z0-9_.])[.][.]/(src|include)/")
                            list(APPEND _pof_gen "${_pof_t}: ${_pof_s}")
                        endif()
                        continue()
                    endif()
                    get_filename_component(_pof_abs "${_pof_s}" ABSOLUTE
                                           BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
                    # Windows paths ignore letter case, so compare folded and with
                    # forward slashes; report the spelling the target used
                    string(REPLACE "\\" "/" _pof_abs "${_pof_abs}")
                    string(TOLOWER "${_pof_abs}" _pof_abs)
                    string(FIND "${_pof_abs}" "${_pof_root_fold}/" _pof_at)
                    if(NOT _pof_at EQUAL 0)
                        continue()
                    endif()
                    string(LENGTH "${_pof_root_fold}/" _pof_cut)
                    string(SUBSTRING "${_pof_abs}" ${_pof_cut} -1 _pof_rel)
                    if(DEFINED "PORT_OVERLAY_FOLD_${_pof_rel}")
                        list(APPEND _pof_raw "${_pof_t}: ${_pof_s}")
                    endif()
                endforeach()
            endif()
        endif()
        foreach(_pof_prop INCLUDE_DIRECTORIES INTERFACE_INCLUDE_DIRECTORIES)
            get_target_property(_pof_dirs ${_pof_t} ${_pof_prop})
            if(NOT _pof_dirs)
                continue()
            endif()
            set(_pof_new "")
            set(_pof_changed FALSE)
            set(_pof_last "")
            foreach(_pof_d IN LISTS _pof_dirs)
                set(_pof_norm "${_pof_d}")
                if(NOT _pof_d MATCHES "^\\$<")
                    get_filename_component(_pof_norm "${_pof_d}" ABSOLUTE
                                           BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
                endif()
                if(_pof_norm STREQUAL _pof_inc AND NOT _pof_last STREQUAL _pof_ovl_inc)
                    list(APPEND _pof_new "${_pof_ovl_inc}")
                    set(_pof_changed TRUE)
                    math(EXPR _pof_inc_n "${_pof_inc_n} + 1")
                endif()
                list(APPEND _pof_new "${_pof_d}")
                set(_pof_last "${_pof_norm}")
            endforeach()
            if(_pof_changed)
                set_property(TARGET ${_pof_t} PROPERTY ${_pof_prop} "${_pof_new}")
            endif()
        endforeach()
    endforeach()
    if(_pof_gen)
        list(JOIN _pof_gen "\n  " _pof_gen_txt)
        message(FATAL_ERROR "DECOMP OVERLAY: target source(s) name decomp src/ or include/ "
                "text through a generator expression, which the overlay cannot reroute or "
                "check:\n  ${_pof_gen_txt}\nName the file directly so port_overlay_reroute() "
                "can send it through the overlay.")
    endif()
    if(_pof_raw)
        list(JOIN _pof_raw "\n  " _pof_raw_txt)
        message(FATAL_ERROR "DECOMP OVERLAY: target source(s) still name the raw decomp file "
                "of a file the overlay patches, so they would compile the unpatched text:\n  "
                "${_pof_raw_txt}\nA source added after port_overlay_reroute() last ran needs "
                "another call (or name it after the reroute before PORTSYNC2).")
    endif()
    get_property(_pof_done GLOBAL PROPERTY PORT_OVERLAY_REROUTED)
    set(_pof_props_n 0)
    foreach(_pof_rel IN LISTS _pof_done)
        foreach(_pof_prop IN LISTS PORT_OVERLAY_SOURCE_PROPS)
            get_source_file_property(_pof_v "${PORT_DECOMP_ROOT}/${_pof_rel}" ${_pof_prop})
            if(NOT _pof_v STREQUAL "NOTFOUND")
                set_source_files_properties("${PORT_OVERLAY_DIR}/${_pof_rel}"
                    PROPERTIES ${_pof_prop} "${_pof_v}")
                math(EXPR _pof_props_n "${_pof_props_n} + 1")
            endif()
        endforeach()
    endforeach()
    list(LENGTH _pof_done _pof_done_n)
    message(STATUS "DECOMP OVERLAY: ${_pof_done_n} decomp source(s) compiled from the overlay, "
                   "${_pof_props_n} source propert(ies) carried onto them; overlay include dir "
                   "placed before the decomp include dir in ${_pof_inc_n} include list(s)")
endfunction()
