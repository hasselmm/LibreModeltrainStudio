## =====================================================================================================================
## Adds a target that builds the project's documentation using sphinx-build.
##
## Arguments
## ---------
##
## - TARGET: Name of the target to generate
## - OPTIONS: Additional commandline options for sphinx-build
## - VERBOSE: Generate verbose output when running sphinx-build
## - SOURCE_DIRECTORY: Directory containing the Sphinx configuration, and documents
##                     (default: "${CMAKE_CURRENT_SOURCE_DIR}/sphinx")
## - OUTPUT_DIRECTORY: Directory into which to output the project documentation
##                     (default: "${CMAKE_CURRENT_BINARY_DIR}/sphinx")
## - DOXYGEN_TARGET: Name of the Doxygen target to depend on (default: "doxygen")
## - DOXYGEN_XML: Location of the generated API documentation in Doxygen's XML format
##                (default: "${DOXYGEN_OUTPUT_DIRECTORY}/xml")
## =====================================================================================================================

function(sphinx_add_docs TARGET)
    set(options VERBOSE)
    set(values PROJECT SOURCE_DIRECTORY OUTPUT_DIRECTORY DOXYGEN_TARGET DOXYGEN_XML)
    set(lists OPTIONS)

    cmake_parse_arguments(PARSE_ARGV 1 SPHINX "${options}" "${values}" "${lists}")

    # Apply default values to options not passed -----------------------------------------------------------------------

    if (NOT SPHINX_PROJECT)
        set(SPHINX_PROJECT auto)
    endif()

    if (NOT SPHINX_VERBOSE)
        list(APPEND SPHINX_OPTIONS -q)
    endif()

    list(PREPEND SPHINX_OPTIONS -b html)

    if (NOT SPHINX_DOXYGEN_TARGET)
        set(SPHINX_DOXYGEN_TARGET doxygen)
    endif()

    if (NOT SPHINX_SOURCE_DIRECTORY)
        set(SPHINX_SOURCE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sphinx)
    endif()

    if (NOT SPHINX_OUTPUT_DIRECTORY)
        set(SPHINX_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/sphinx)
    endif()

    if (NOT TARGET ${SPHINX_DOXYGEN_TARGET})
        message(FATAL_ERROR [[Invalid Doxygen target: "${SPHINX_DOXYGEN_TARGET}"]])
    endif()

    if (NOT SPHINX_DOXYGEN_XML)
        if (NOT DOXYGEN_OUTPUT_DIRECTORY)
            message(FATAL_ERROR [[Cannot guess default value for DOXYGEN_XML parameter: Variable DOXYGEN_OUTPUT_DIRECTORY is empty]])
        endif()

        set(SPHINX_DOXYGEN_XML ${DOXYGEN_OUTPUT_DIRECTORY}/xml)
    endif()

    # Create the custom CMake target -----------------------------------------------------------------------------------

    add_custom_target(
        ${TARGET} DEPENDS ${SPHINX_DOXYGEN_TARGET}
        COMMENT "Running Sphinx to generate project documentation..."
        COMMAND Sphinx::Build ${SPHINX_OPTIONS}
        -Dbreathe_projects.${SPHINX_PROJECT}=${SPHINX_DOXYGEN_XML}
        ${SPHINX_SOURCE_DIRECTORY} ${SPHINX_OUTPUT_DIRECTORY}
    )
endfunction()

## =====================================================================================================================
## This internal function checks, if a Sphinx extension is available
## =====================================================================================================================

function(_sphinx_find_extension EXTENSION INSTALL_CHECK)
    execute_process(
        COMMAND ${Python3_INTERPRETER} -c "${INSTALL_CHECK}"
        RESULT_VARIABLE EXTENSION_FOUND
        OUTPUT_VARIABLE EXTENSION_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    if (EXTENSION_FOUND EQUAL 0)
        set(Sphinx_${EXTENSION}_FOUND TRUE PARENT_SCOPE)
        set(Sphinx_${EXTENSION}_VERSION ${EXTENSION_VERSION} PARENT_SCOPE)
    else()
        set(Sphinx_${EXTENSION}_FOUND FALSE PARENT_SCOPE)
    endif()

    list(APPEND Sphinx_EXTENSIONS ${EXTENSION})
    set(Sphinx_EXTENSIONS ${Sphinx_EXTENSIONS} PARENT_SCOPE)
endfunction()

## =====================================================================================================================
## Look for an executable called sphinx-build
## =====================================================================================================================

find_program(
    SPHINX_EXECUTABLE
    PATHS /usr/bin C:/msys64/usr/bin C:/msys64/mingw64/bin C:/msys64/clang64/bin
    NAMES sphinx-build sphinx-build.exe
    DOC "Path to sphinx-build executable")

if (SPHINX_EXECUTABLE)
    execute_process(
        COMMAND ${SPHINX_EXECUTABLE} --version
        OUTPUT_VARIABLE SPHINX_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY)

    string(REGEX REPLACE ".* " "" SPHINX_VERSION ${SPHINX_VERSION})
endif()

## =====================================================================================================================
## Search extensions
## =====================================================================================================================

find_package(Python3 COMPONENTS Interpreter)
if (Python3_FOUND AND Python3_Interpreter_FOUND)
    get_target_property(Python3_INTERPRETER Python3::Interpreter IMPORTED_LOCATION)

    set(Sphinx_EXTENSIONS)
    _sphinx_find_extension(Breathe "import breathe; print(breathe.Sphinx and breathe.__version__)")
    _sphinx_find_extension(MystParser "import myst_parser.sphinx_ext; print(myst_parser.__version__)")
    _sphinx_find_extension(QtHelp "import sphinxcontrib.qthelp; print(sphinxcontrib.qthelp.__version__)")
endif()

## =====================================================================================================================
## Handle standard arguments to find_package like REQUIRED and QUIET
## =====================================================================================================================

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
    Sphinx
    FAIL_MESSAGE "Failed to find Sphinx documenation generator"
    REQUIRED_VARS SPHINX_EXECUTABLE
    HANDLE_COMPONENTS
)

## =====================================================================================================================
## Report results
## =====================================================================================================================

if (SPHINX_FOUND)
    add_executable(Sphinx::Build IMPORTED)
    set_target_properties(Sphinx::Build PROPERTIES IMPORTED_LOCATION ${SPHINX_EXECUTABLE})

    if (NOT Sphinx_FIND_QUIETLY)
        message(STATUS "Using Sphinx: ${SPHINX_EXECUTABLE} (version ${SPHINX_VERSION})")

        list(APPEND CMAKE_MESSAGE_INDENT "  ")

        foreach(extension ${Sphinx_EXTENSIONS})
            if (Sphinx_${extension}_FOUND)
                message(STATUS "${extension} extension found (version: ${Sphinx_${extension}_VERSION})")
            endif()
        endforeach()

        list(POP_BACK CMAKE_MESSAGE_INDENT)
    endif()
endif()
