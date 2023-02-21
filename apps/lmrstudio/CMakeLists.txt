qt_add_executable (
    LmrsStudio WIN32

    accessorycontrolview.cpp
    accessorycontrolview.h
    accessorytablemodel.cpp
    accessorytablemodel.h
    automationview.cpp
    automationview.h
    debugview.cpp
    debugview.h
    decoderdatabaseview.cpp
    decoderdatabaseview.h
    detectorinfotablemodel.cpp
    detectorinfotablemodel.h
    deviceconnectionview.cpp
    deviceconnectionview.h
    deviceparameterwidget.cpp
    deviceparameterwidget.h
    functionmappingview.cpp
    functionmappingview.h
    main.cpp
    main.qrc
    mainwindow.cpp
    mainwindow.h
    multideviceview.cpp
    multideviceview.h
    speedmeterview.cpp
    speedmeterview.h
    trackplanview.cpp
    trackplanview.h
    variableeditorview.cpp
    variableeditorview.h
    vehiclecontrolview.cpp
    vehiclecontrolview.h
)

target_link_libraries(
    LmrsStudio
    PRIVATE Qt6::Charts Qt6::QuickWidgets
    PRIVATE Lmrs::Widgets
    PRIVATE Lmrs::Esu Lmrs::KpfZeller Lmrs::Roco Lmrs::Zimo
)

set_target_properties(
    LmrsStudio PROPERTIES
    FOLDER "qtc_runnable"
)

lmrs_doxygen_add_target(LmrsStudio)
lmrs_add_translations(LmrsStudio)
lmrs_install_target(LmrsStudio)
