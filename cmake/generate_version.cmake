# Writes core/version.h (see src/code/core/version.h.in) from git - the last tag, the commit, the branch,
# whether the tree is dirty - plus a UTC build timestamp. Run at build time by the ab_version target, so a
# new commit shows up in the next build, not the next configure; configure_file only rewrites the header
# when something changed, so nothing recompiles needlessly. The timestamp is the time the *facts* last
# changed, not of this run: it is kept from the existing header while tag, hash, branch and dirty flag are
# the same, or a no-change ninja run would still recompile everything that includes the header and relink
# every program (25 s on the PC for nothing). A release is a clean tree built once, so its stamp is real.
#
# Where there is no .git (the console build server gets an rsync without it - see make_psc.sh), the
# AB_GIT_VERSION / AB_GIT_HASH / AB_GIT_BRANCH / AB_GIT_DIRTY environment variables say what the tree is.
#
# Expects SOURCE_DIR, BINARY_DIR and VERSION_FALLBACK to be passed with -D.

if (DEFINED ENV{AB_GIT_HASH} AND NOT "$ENV{AB_GIT_HASH}" STREQUAL "")
    set(GIT_HASH "$ENV{AB_GIT_HASH}")
    set(GIT_BRANCH "$ENV{AB_GIT_BRANCH}")
    set(GIT_VERSION "$ENV{AB_GIT_VERSION}")
    if ("$ENV{AB_GIT_DIRTY}" STREQUAL "true")
        set(GIT_DIRTY "true")
    else()
        set(GIT_DIRTY "false")
    endif()
else()
    execute_process(COMMAND git rev-parse --short HEAD
            WORKING_DIRECTORY ${SOURCE_DIR} OUTPUT_VARIABLE GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND git rev-parse --abbrev-ref HEAD
            WORKING_DIRECTORY ${SOURCE_DIR} OUTPUT_VARIABLE GIT_BRANCH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND git describe --tags --abbrev=0
            WORKING_DIRECTORY ${SOURCE_DIR} OUTPUT_VARIABLE GIT_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    # diff-index trusts the index's cached stat info; without a refresh first, a file whose mtime changed
    # (a merge, a checkout, another git build - MSYS2's git and Git for Windows keep different stat data) is
    # reported as modified when its content is not, and the build is stamped "dirty" for nothing
    execute_process(COMMAND git update-index -q --refresh
            WORKING_DIRECTORY ${SOURCE_DIR} RESULT_VARIABLE _ignored ERROR_QUIET OUTPUT_QUIET)
    execute_process(COMMAND git diff-index --quiet HEAD --
            WORKING_DIRECTORY ${SOURCE_DIR} RESULT_VARIABLE GIT_DIRTY_RESULT ERROR_QUIET)
    if (GIT_HASH AND NOT GIT_DIRTY_RESULT EQUAL 0)
        set(GIT_DIRTY "true")
    else()
        set(GIT_DIRTY "false")
    endif()
endif()

if (NOT GIT_VERSION)
    set(GIT_VERSION "${VERSION_FALLBACK}")
endif()
if (NOT GIT_HASH)
    set(GIT_HASH "unknown")
endif()
if (NOT GIT_BRANCH)
    set(GIT_BRANCH "unknown")
endif()
if (GIT_DIRTY STREQUAL "true")
    set(GIT_DIRTY_FLAG "*")
else()
    set(GIT_DIRTY_FLAG "")
endif()

set(VERSION_HEADER ${BINARY_DIR}/generated/core/version.h)
set(BUILD_TIMESTAMP "")
if (EXISTS ${VERSION_HEADER})
    file(READ ${VERSION_HEADER} OLD_HEADER)
    set(OLD_FACTS "")
    foreach (name VERSION GIT_HASH GIT_BRANCH GIT_DIRTY_FLAG)
        string(REGEX MATCH "[*]${name} = \"([^\"]*)\"" _m "${OLD_HEADER}")
        set(OLD_FACTS "${OLD_FACTS}|${CMAKE_MATCH_1}")
    endforeach()
    if (OLD_FACTS STREQUAL "|${GIT_VERSION}|${GIT_HASH}|${GIT_BRANCH}|${GIT_DIRTY_FLAG}")
        string(REGEX MATCH "BUILD_TIMESTAMP = \"([^\"]*)\"" _m "${OLD_HEADER}")
        set(BUILD_TIMESTAMP "${CMAKE_MATCH_1}")
    endif()
endif()
if (BUILD_TIMESTAMP STREQUAL "")
    string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S" UTC)
endif()

configure_file(${SOURCE_DIR}/src/code/core/version.h.in ${BINARY_DIR}/generated/core/version.h @ONLY)
