cmake_minimum_required(VERSION 3.20)

project(
    "Libre Modelrail Studio" VERSION "0.0.1"
    HOMEPAGE_URL "https://github.com/hasselmm/LibreModeltrainStudio"
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(LmrsSupport)

enable_testing()
include(CPack)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(
    Qt6
    COMPONENTS CoreTools Charts LinguistTools Multimedia QuickWidgets SerialPort Sql Svg Test Xml
    OPTIONAL_COMPONENTS ZlibPrivate
    QUIET REQUIRED
)

find_package(WrapOpenSSL QUIET REQUIRED)

if (TARGET Qt6::ZlibPrivate)
    message(STATUS "Using Qt6::ZlibPrivate")
    add_library(Lmrs::Zlib ALIAS Qt6::ZlibPrivate)
else()
    find_package(WrapZLIB QUIET REQUIRED)
    add_library(Lmrs::Zlib ALIAS WrapZLIB::WrapZLIB)
endif()

qt_standard_project_setup()

add_definitions(
    -DLMRS_VERSION_STRING=\"${CMAKE_PROJECT_VERSION}\"_L1
    -DLMRS_HOMEPAGE_URL=\"${CMAKE_PROJECT_HOMEPAGE_URL}\"_L1
    -DQT_DEPRECATED_WARNINGS_SINCE=QT_VERSION_CHECK\(6,4,0\)
    -DQT_NO_CAST_FROM_ASCII=1
    -DQT_NO_URL_CAST_FROM_STRING=1
    -DQT_STRICT_ITERATORS=1
)

add_compile_options(
    -Wall
    -Wextra
    -Werror=conversion
    -Werror=format
    -Werror=ignored-qualifiers
    -Werror=implicit-fallthrough
    -Werror=missing-declarations
    -Werror=missing-field-initializers
    -Werror=pessimizing-move
    -Werror=reorder
    -Werror=return-type
    -Werror=sign-compare
    -Werror=switch
    -Werror=switch-enum
    -Werror=tautological-compare
    -Werror=unused
    -Werror=unused-parameter
    -Werror=unused-result
)

if (CLANG)
    add_compile_options(
        -Werror=missing-prototypes
    )
endif()

find_package(CompilerCache)
find_package(Doxygen)
find_package(Sphinx COMPONENTS Breathe MystParser QtHelp)

add_custom_target(documentation SOURCES README.md)

add_subdirectory(lmrs)

add_subdirectory(apps)
add_subdirectory(docs)
add_subdirectory(tools)
add_subdirectory(tests)

file(GLOB_RECURSE cmake_sources CONFIGURE_DEPENDS cmake/*.cmake)
add_custom_target (cmake SOURCES ${cmake_sources})

file(GLOB_RECURSE github_sources CONFIGURE_DEPENDS .github/**/*.yml)
add_custom_target (github SOURCES ${github_sources})
