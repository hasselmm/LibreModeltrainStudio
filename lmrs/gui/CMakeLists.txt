add_library (
    LmrsGui STATIC
    fontawesome.cpp
    fontawesome.h
    fontawesome.qrc
    localization.cpp
    localization.h
    symbolfonts.cpp
    symbolfonts.h
)

target_include_directories(LmrsGui PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(LmrsGui PUBLIC Lmrs::Core Qt6::Gui)

add_library(Lmrs::Gui ALIAS LmrsGui)
lmrs_doxygen_add_target(LmrsGui)
lmrs_add_translations(LmrsGui)
