file(GLOB_RECURSE thirdparty CONFIGURE_DEPENDS thirdparty/*.pdf)
add_custom_target(thirdparty SOURCES ${thirdparty})

if (DOXYGEN_FOUND)
    message(STATUS "Generating API documentation using: ${DOXYGEN_EXECUTABLE} (version: ${DOXYGEN_VERSION})")

    set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doxygen)
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE ../README.md)
    set(DOXYGEN_EXCLUDE lmrs/gui/fontawesome/)
    set(DOXYGEN_GENERATE_HTML YES)
    set(DOXYGEN_GENERATE_XML YES)

    set(DOXYGEN_WARN_IF_UNDOCUMENTED NO) # FIXME at some point reverse is setting, but > 3'200 warnings are too much
    set(DOXYGEN_JAVADOC_AUTOBRIEF YES)
    set(DOXYGEN_QT_AUTOBRIEF YES)

    get_property(doxygen_sources GLOBAL PROPERTY LMRS_DOXYGEN_SOURCES)

    doxygen_add_docs(
        doxygen ${doxygen_sources} ../README.md USE_STAMP_FILE
        COMMENT "Running Doxygen to generate API documentation...")

    if (SPHINX_FOUND)
        sphinx_add_docs(sphinx PROJECT lmrs DOXYGEN_TARGET doxygen)
    endif()
endif()

if (NOT TARGET sphinx)
    add_custom_target(sphinx)
endif()

target_sources(
    sphinx PRIVATE
    sphinx/conf.py
    sphinx/index.rst
    sphinx/links.rst
    sphinx/readme.rst
    sphinx/core/index.rst
    sphinx/gui/index.rst
    sphinx/studio/index.rst
    sphinx/widgets/index.rst
    sphinx/vendor/index.rst
    sphinx/vendor/esu.rst
    sphinx/vendor/kpfzeller.rst
    sphinx/vendor/roco.rst
    sphinx/vendor/zimo.rst
)

