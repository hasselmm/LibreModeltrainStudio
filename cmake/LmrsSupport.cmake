set(LRMS_LANGUAGES de en)

## =====================================================================================================================
## This function adds translations to the specified TARGET
## =====================================================================================================================

function(lmrs_add_translations TARGET)
    # build list of translation files
    set(translations)
    string(TOLOWER ${TARGET} target_id)

    foreach(lang_id IN ITEMS ${LRMS_LANGUAGES})
        list(APPEND translations i18n/${target_id}.${lang_id}.ts)
    endforeach()

    target_sources(${TARGET} PRIVATE ${translations})

    # build nice resource namespace
    string(REPLACE "Lmrs" "lmrs/" namespace ${TARGET})
    string(TOLOWER ${namespace} namespace)

    # add QML files to the sources list
    get_target_property(sources ${TARGET} SOURCES)
    file(GLOB_RECURSE qml_sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} qml/*.qml)
    list(APPEND sources ${qml_sources})

    qt_add_translations(
        ${TARGET} TS_FILES ${translations} SOURCES ${sources}
        RESOURCE_PREFIX "/taschenorakel.de/${namespace}/l18n"
        LUPDATE_OPTIONS -source-language en_US -locations none -tr-function-alias tr+=LMRS_TR
    )
endfunction()

## =====================================================================================================================
## This function installs the Qt runtime for the specified TARGET
## =====================================================================================================================

function(lmrs_install_qt_runtime TARGET)
    # Install instructions must be written to a CMake script as it depends on generator expressions
    set(deployment_script "${CMAKE_CURRENT_BINARY_DIR}/deploy_${TARGET}.cmake")

    # Apple is special; who would have thought that
    if (APPLE)
        set(executable_path "$<TARGET_FILE_NAME:${TARGET}>.app")
    else()
        set(executable_path "\${QT_DEPLOY_BIN_DIR}/$<TARGET_FILE_NAME:${TARGET}>")
    endif()

    file(GENERATE OUTPUT ${deployment_script} CONTENT "
        include(\"${QT_DEPLOY_SUPPORT}\")
        qt_deploy_runtime_dependencies(
            EXECUTABLE \"${executable_path}\"
            GENERATE_QT_CONF
        )"
    )

    # Add the generated script to the install procedure
    install(SCRIPT ${deployment_script})
endfunction()

## =====================================================================================================================
## This function installs the TARGET and alls of its runtime dependencies
## =====================================================================================================================

function(lmrs_install_target TARGET)
    # Resolve binary locations of C++ runtime and Qt
    get_target_property(qtcore_location Qt6::Core LOCATION)

    get_filename_component(compiler_bindir ${CMAKE_CXX_COMPILER} DIRECTORY)
    get_filename_component(qtcore_bindir ${CMAKE_CXX_COMPILER} DIRECTORY)

    # Install the target and the Qt runtime
    install(TARGETS ${TARGET})

    # Still have to figure out what makes sense to bundle for Linux.
    # Probably best to generate DEB and RPM archives instead of bundling "half" of Github's Linux runtime.
    if (NOT CMAKE_SYSTEM_NAME MATCHES Linux)
        lmrs_install_qt_runtime(${TARGET})
    endif()

    # Install runtime dependencies of all the targets
    get_target_property(link_libraries ${TARGET} LINK_LIBRARIES)
    foreach(target LmrsStudio ${link_libraries})
        # CMake cannot resolve runtime dependencies for alias targets, therefore resolve the alias
        get_target_property(aliased ${target} ALIASED_TARGET)

        if (aliased)
            set(target ${aliased})
        endif()

        # Static libraries usually do not report their runtime dependencies, therefore skip them
        get_target_property(type ${target} TYPE)

        if (type MATCHES STATIC)
            continue()
        endif()

        # Now pass runtime dependencies of the currently inspected target to CPack
        install(
            IMPORTED_RUNTIME_ARTIFACTS ${target}
            RUNTIME_DEPENDENCY_SET ${target}::Dependencies
        )

        install(
            RUNTIME_DEPENDENCY_SET ${target}::Dependencies
            PRE_EXCLUDE_REGEXES ^api-ms-.*;^ext-ms-.*;^hvsifiletrust;qt6
            POST_EXCLUDE_REGEXES kernel32;shell32;system32;windows
            DIRECTORIES ${compiler_bindir} ${qtcore_bindir}
        )
    endforeach()
endfunction()

## =====================================================================================================================
## This function adds the sources of a CMake target to the list of files processed by Doxygen
## =====================================================================================================================

function(lmrs_doxygen_add_target TARGET)
    set(doxygen_sources_list ${ARG1})

    if (NOT doxygen_sources_list)
        set(doxygen_sources_list LMRS_DOXYGEN_SOURCES)
    endif()

    get_target_property(sources ${TARGET} SOURCES)
    set(doxygen_sources)

    foreach(filename ${sources})
        file(RELATIVE_PATH filepath ${CMAKE_SOURCE_DIR}/docs ${CMAKE_CURRENT_SOURCE_DIR}/${filename})
        list(APPEND doxygen_sources ${filepath})
    endforeach()

    set_property(GLOBAL APPEND PROPERTY ${doxygen_sources_list} ${doxygen_sources})
endfunction()

## =====================================================================================================================
## This macros shows the current content of the variables passed
## =====================================================================================================================

macro(lmrs_show VARIABLES)
    foreach(name ${VARIABLES} ${ARGN})
        if (CMAKE_CURRENT_FUNCTION)
            message(STATUS "${name} (${CMAKE_CURRENT_FUNCTION}): ${${name}}")
        else()
            message(STATUS "${name}: ${${name}}")
        endif()
    endforeach()
endmacro()
