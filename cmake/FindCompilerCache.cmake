## =====================================================================================================================
## Try to find CCache first: It's simply much faster than the more featureful SCCache
## =====================================================================================================================

find_program(
    CCACHE_EXECUTABLE ccache PATHS
    "[HKLM/SOFTWARE/Microsoft/Windows/CurrentVersion;ProgramFilesDir]/CCache"
    "[HKLM/SOFTWARE/Microsoft/Windows/CurrentVersion;ProgramFilesDir (x86)]/CCache"
    DOC "Path of the CCache executable (https://ccache.dev/)")

if (CCACHE_EXECUTABLE)
    execute_process(
        COMMAND ${CCACHE_EXECUTABLE} --version
        OUTPUT_VARIABLE CCACHE_VERSION
        COMMAND_ERROR_IS_FATAL ANY)

    string(REPLACE "\n" ";" CCACHE_VERSION ${CCACHE_VERSION})
    list(GET CCACHE_VERSION 0 CCACHE_VERSION)
    string(REGEX REPLACE ".*version +" "" CCACHE_VERSION ${CCACHE_VERSION})

    set(CompilerCache_EXECUTABLE ${CCACHE_EXECUTABLE})
    set(CompilerCache_TYPE CCACHE)
else()
    ## =================================================================================================================
    ## Give SCCache a chance, if CCache could not be found
    ## =================================================================================================================

    find_program(
        SCCACHE_EXECUTABLE sccache PATHS
        "[HKLM/SOFTWARE/Microsoft/Windows/CurrentVersion;ProgramFilesDir]/SCCache"
        "[HKLM/SOFTWARE/Microsoft/Windows/CurrentVersion;ProgramFilesDir (x86)]/SCCache"
        DOC "Path of the SCCache executable (https://github.com/mozilla/sccache)")

    if (SCCACHE_EXECUTABLE)
        execute_process(
            COMMAND ${SCCACHE_EXECUTABLE} --version
            OUTPUT_VARIABLE SCCACHE_VERSION
            COMMAND_ERROR_IS_FATAL ANY)

        string(REPLACE "\n" ";" SCCACHE_VERSION ${SCCACHE_VERSION})
        list(GET SCCACHE_VERSION 0 SCCACHE_VERSION)
        string(REGEX REPLACE ".*sccache +" "" SCCACHE_VERSION ${SCCACHE_VERSION})

        message(STATUS "Using SCCache (version ${SCCACHE_VERSION} from ${SCCACHE_EXECUTABLE})")

        set(CompilerCache_EXECUTABLE ${SCCACHE_EXECUTABLE})
        set(CompilerCache_TYPE SCCACHE)
    endif()
endif()

## =====================================================================================================================
## Handle standard arguments to find_package like REQUIRED and QUIET
## =====================================================================================================================

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
    CompilerCache
    FAIL_MESSAGE "Failed to find any supported compiler cache tool"
    REQUIRED_VARS CompilerCache_EXECUTABLE CompilerCache_TYPE
    HANDLE_COMPONENTS
)

if (CompilerCache_FOUND)
    if (NOT CompilerCache_FIND_QUIETLY)
        if (CompilerCache_TYPE STREQUAL CCACHE)
            message(STATUS "Using CCache: ${CCACHE_EXECUTABLE} (version: ${CCACHE_VERSION})")
        elseif (CompilerCache_TYPE STREQUAL SCACHE)
            message(STATUS "Using SCCache: ${SCCACHE_EXECUTABLE} (version: ${SCCACHE_VERSION})")
        endif()
    endif()

    foreach(lang in C CXX)
        if (CMAKE_${lang}_COMPILER AND NOT CMAKE_${lang}_COMPILER_LAUNCHER)
            set(CMAKE_${lang}_COMPILER_LAUNCHER ${CompilerCache_EXECUTABLE}
                CACHE FILEPATH "Path of the compiler cache executable for ${LANG}")
            message(STATUS "Enabling compiler cache for ${lang}: ${CMAKE_${lang}_COMPILER_LAUNCHER}")
        endif()
    endforeach()
endif()
