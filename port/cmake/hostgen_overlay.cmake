# Run port/tools/hostgen.py on the overlay copy of a decomp source when the patch
# layer has one, and on the decomp's own file otherwise (run catchup1, Task 1.3).
#
#   cmake -DPORT_OVERLAY_DIR=<dir> -DPORT_HOSTGEN_PY=<hostgen.py> -P hostgen_overlay.cmake -- <hostgen args>
#
# CMakeLists.txt spells every hostgen rule `COMMAND ${PORT_HOSTGEN} <args>`
# (port/cmake/decomp_overlay.cmake defines it and refuses a rule that calls
# hostgen.py directly). hostgen takes `--decomp <root>` and reads <root>/<path>;
# the overlay dir mirrors the decomp's relative paths, so the generated copy
# lands at the same host-src/<path>.cpp with the same header either way.

set(_hg_args "")
set(_hg_on FALSE)
math(EXPR _hg_last "${CMAKE_ARGC} - 1")
foreach(_hg_i RANGE ${_hg_last})
    if(_hg_on)
        list(APPEND _hg_args "${CMAKE_ARGV${_hg_i}}")
    elseif("${CMAKE_ARGV${_hg_i}}" STREQUAL "--")
        set(_hg_on TRUE)
    endif()
endforeach()
if(NOT PORT_HOSTGEN_PY OR NOT PORT_OVERLAY_DIR)
    message(FATAL_ERROR "hostgen_overlay: PORT_HOSTGEN_PY and PORT_OVERLAY_DIR must be set")
endif()
# hostgen's --decomp default is the configured decomp root (port/tools/decomp_root.py reads
# SM64DS_DECOMP_ROOT); ninja runs this script in its own environment, so hand it over here.
if(PORT_DECOMP_ROOT)
    set(ENV{SM64DS_DECOMP_ROOT} "${PORT_DECOMP_ROOT}")
endif()

set(_hg_manifest "${PORT_OVERLAY_DIR}/overlay_manifest.txt")
set(_hg_listed "")
if(EXISTS "${_hg_manifest}")
    file(STRINGS "${_hg_manifest}" _hg_listed)
endif()
foreach(_hg_rel IN LISTS _hg_listed)
    set("_hg_has_${_hg_rel}" 1)
endforeach()

set(_hg_common "")
set(_hg_raw "")
set(_hg_ovl "")
set(_hg_take "")
set(_hg_own_root FALSE)
foreach(_hg_a IN LISTS _hg_args)
    if(_hg_take)
        list(APPEND _hg_common "${_hg_a}")
        set(_hg_take "")
    elseif(_hg_a STREQUAL "--out" OR _hg_a STREQUAL "--decomp")
        list(APPEND _hg_common "${_hg_a}")
        set(_hg_take "${_hg_a}")
        if(_hg_a STREQUAL "--decomp")
            set(_hg_own_root TRUE)
        endif()
    elseif(_hg_a MATCHES "^--")
        list(APPEND _hg_common "${_hg_a}")
    elseif(_hg_a MATCHES "[/\\\\]")
        string(REPLACE "\\" "/" _hg_a "${_hg_a}")
        if(DEFINED "_hg_has_${_hg_a}")
            list(APPEND _hg_ovl "${_hg_a}")
        else()
            list(APPEND _hg_raw "${_hg_a}")
        endif()
    else()
        # A bare symbol: hostgen finds it by name under src/. Refuse it when the
        # overlay holds a source of that name, since the name says nothing about
        # which copy hostgen would open.
        foreach(_hg_rel IN LISTS _hg_listed)
            get_filename_component(_hg_stem "${_hg_rel}" NAME_WE)
            if(_hg_rel MATCHES "^src/" AND _hg_stem STREQUAL _hg_a)
                message(FATAL_ERROR "hostgen_overlay: '${_hg_a}' is a bare name and the overlay "
                        "has ${_hg_rel}; pass the repo-relative path so the patched copy is read")
            endif()
        endforeach()
        list(APPEND _hg_raw "${_hg_a}")
    endif()
endforeach()

if(_hg_own_root AND _hg_ovl)
    message(FATAL_ERROR "hostgen_overlay: --decomp given with overlay-backed sources ${_hg_ovl}")
endif()
if(_hg_raw OR NOT _hg_ovl)
    execute_process(COMMAND python "${PORT_HOSTGEN_PY}" ${_hg_common} ${_hg_raw}
                    RESULT_VARIABLE _hg_rc)
    if(NOT _hg_rc EQUAL 0)
        message(FATAL_ERROR "hostgen_overlay: hostgen.py failed (${_hg_rc}) on ${_hg_raw}")
    endif()
endif()
foreach(_hg_rel IN LISTS _hg_ovl)
    execute_process(COMMAND python "${PORT_HOSTGEN_PY}" ${_hg_common}
                            --decomp "${PORT_OVERLAY_DIR}" "${_hg_rel}"
                    RESULT_VARIABLE _hg_rc)
    if(NOT _hg_rc EQUAL 0)
        message(FATAL_ERROR "hostgen_overlay: hostgen.py failed (${_hg_rc}) on the overlay copy "
                "of ${_hg_rel}")
    endif()
    message("hostgen_overlay: ${_hg_rel} generated from ${PORT_OVERLAY_DIR}/${_hg_rel}")
endforeach()
