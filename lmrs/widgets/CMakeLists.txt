add_library (
    LmrsWidgets STATIC

    actionutils.cpp
    actionutils.h
    navigationtoolbar.cpp
    navigationtoolbar.h
    recentfilemenu.cpp
    recentfilemenu.h
    speeddial.cpp
    speeddial.h
    speedmeter.cpp
    speedmeter.h
    spinbox.cpp
    spinbox.h
    statusbar.cpp
    statusbar.h
    symbolictrackplanview.cpp
    symbolictrackplanview.h
    widgets.qrc
)

target_include_directories(LmrsWidgets PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(
    LmrsWidgets
    PUBLIC Lmrs::Gui Qt6::Widgets
    PRIVATE Qt6::Svg Qt6::QuickWidgets Qt6::Xml
)

add_library(Lmrs::Widgets ALIAS LmrsWidgets)
lmrs_add_translations(LmrsWidgets)
