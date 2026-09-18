# Writes core/version.h (see src/code/core/version.h.in) from git - the last tag, the commit, the branch,
# whether the tree is dirty - plus a UTC build timestamp. Run at build time by the ab_version target, so a
# new commit shows up in the next build, not the next configure; configure_file only rewrites the header
# when something changed, so nothing recompiles needlessly.
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

string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S" UTC)

configure_file(${SOURCE_DIR}/src/code/core/version.h.in ${BINARY_DIR}/generated/core/version.h @ONLY)
