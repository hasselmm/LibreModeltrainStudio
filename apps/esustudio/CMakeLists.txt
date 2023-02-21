add_definitions (
    -DQT_RESTRICTED_CAST_FROM_ASCII=1
    -UQT_NO_CAST_FROM_ASCII
)

qt_add_executable (
    EsuStudio WIN32

    audiopreview.cpp
    audiopreview.h
    assets.qrc
    decoderinfo.cpp
    decoderinfo.h
    documentpreview.cpp
    documentpreview.h
    functioneditor.cpp
    functioneditor.h
    functiontablemodel.cpp
    functiontablemodel.h
    imagepreview.cpp
    imagepreview.h
    main.cpp
    mainwindow.cpp
    mainwindow.h
    metadata.cpp
    metadata.h
    package.cpp
    package.h
    packageexplorer.cpp
    packageexplorer.h
    packagelistmodel.cpp
    packagelistmodel.h
    packagesummary.cpp
    packagesummary.h
    soundsloteditor.cpp
    soundsloteditor.h
    soundslottablemodel.cpp
    soundslottablemodel.h
)

target_link_libraries(
    EsuStudio

    PRIVATE
    Lmrs::Widgets
    Lmrs::Zlib
    Qt6::Multimedia
    Qt6::Widgets
    Qt6::Xml
    WrapOpenSSL::WrapOpenSSL
)

set_target_properties(
    EsuStudio PROPERTIES
    FOLDER "qtc_runnable"
)
