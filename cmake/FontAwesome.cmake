cmake_minimum_required(VERSION 3.20)

if (NOT SOURCE OR NOT TARGET)
    message(STATUS "Produces a C++ header file from FontAwesomes icons.json.")
    message(STATUS "Usage: ${CMAKE_COMMAND} -DSOURCE=... -DTARGET=... -P${CMAKE_CURRENT_LIST_FILE}")
    message(FATAL_ERROR "Required arguments missing.")
endif()

file(READ "${SOURCE}" fontawesome)

set(header
    "\#ifndef LMRS_GUI_FONTAWESOME_H\n"
    "\#define LMRS_GUI_FONTAWESOME_H\n"
    "\n"
    "\#include <lmrs/core/typetraits.h>\n"
    "\n"
    "\#include <QChar>\n"
    "\n"
    "class QIcon\;\n"
    "\n"
    "namespace lmrs::gui::fontawesome {\n"
    "\n"
    "using BrandsIcon = core::Literal<QChar, struct BrandsIconTag>\;\n"
    "using RegularIcon = core::Literal<QChar, struct RegularIconTag>\;\n"
    "using SolidIcon = core::Literal<QChar, struct SolidIconTag>\;\n"
    "\n"
    "QIcon icon(BrandsIcon icon)\;\n"
    "QIcon icon(SolidIcon icon)\;\n"
    "QIcon icon(RegularIcon icon)\;\n"
    "\n"
)

string(JSON icon_count LENGTH ${fontawesome})
message(STATUS "Processing ${icon_count} icons...")

foreach(i RANGE 1 ${icon_count})
    math(EXPR i "${i} - 1")
    math(EXPR m "${i} % 42")

    if (i AND m EQUAL 0)
        math(EXPR p "100 * ${i} / ${icon_count}")
        message(STATUS "${i}/${icon_count} icons processed (${p}%)...")
    endif()

    string(JSON name MEMBER ${fontawesome} ${i})
    string(JSON unicode GET ${fontawesome} ${name} unicode)
    string(JSON style_count LENGTH ${fontawesome} ${name} free)

    string(REGEX REPLACE "^.*(....)\$" "\\1" unicode "0000${unicode}")

    set(identifier)
    string(REGEX REPLACE "-(.)" ";\\1:" identifier_tokens "-${name}")
    foreach(tokens IN LISTS identifier_tokens)
        if (tokens)
            string(REPLACE ":" ";" tokens "${tokens}")
            list(GET tokens 0 prefix)
            list(GET tokens 1 suffix)
            string(TOUPPER "${prefix}" prefix)
            string(APPEND identifier "${prefix}${suffix}")
        endif()
    endforeach()

    foreach(j RANGE 1 ${style_count})
        math(EXPR j "${j} - 1")
        string(JSON style GET ${fontawesome} ${name} free ${j})
        if (style STREQUAL brands)
            string(APPEND header "constexpr auto fab${identifier} = BrandsIcon{u'\\u${unicode}'}\;\n")
        elseif (style STREQUAL regular)
            string(APPEND header "constexpr auto far${identifier} = RegularIcon{u'\\u${unicode}'}\;\n")
        elseif (style STREQUAL solid)
            string(APPEND header "constexpr auto fas${identifier} = SolidIcon{u'\\u${unicode}'}\;\n")
        else()
            message(FATAL_ERROR "Unknown FontAwesome style: ${style}")
        endif()
    endforeach()
endforeach()

string(APPEND header
    "\n"
    "} // namespace lmrs::gui::fontawesome\n"
    "\n"
    "\#endif // LMRS_GUI_FONTAWESOME_H\n"
)

message(STATUS "Finished. Writing \"${TARGET}\"...")

list(JOIN header "" header)
file(WRITE "${TARGET}" "${header}")
