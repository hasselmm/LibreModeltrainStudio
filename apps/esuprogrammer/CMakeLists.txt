qt_add_executable (
    EsuProgrammer WIN32

    lokprogrammer.cpp
    lokprogrammer.h
    main.cpp
    main.qrc
    mainwindow.cpp
    mainwindow.h
)

target_link_libraries(
    EsuProgrammer
    PRIVATE Lmrs::Esu Qt6::SerialPort Qt6::Widgets
)

set_target_properties(
    EsuProgrammer PROPERTIES
    FOLDER "qtc_runnable"
)
