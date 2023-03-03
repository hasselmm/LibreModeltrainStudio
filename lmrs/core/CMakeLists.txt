add_library (
    LmrsCore STATIC

    accessories.cpp
    accessories.h
    algorithms.cpp
    algorithms.h
    automationmodel.cpp
    automationmodel.h
    continuation.cpp
    continuation.h
    dccconstants.cpp
    dccconstants.h
    dccrequest.cpp
    dccrequest.h
    decoderinfo.cpp
    decoderinfo.h
    decoderinfo.qrc
    detectors.cpp
    detectors.h
    device.cpp
    device.h
    fileformat.cpp
    fileformat.h
    framestream.cpp
    framestream.h
    localization.cpp
    localization.h
    logging.cpp
    logging.h
    memory.cpp
    memory.h
    parameters.cpp
    parameters.h
    propertyguard.cpp
    propertyguard.h
    quantities.cpp
    quantities.h
    staticinit.cpp
    staticinit.h
    symbolictrackplanmodel.cpp
    symbolictrackplanmodel.h
    typetraits.cpp
    typetraits.h
    userliterals.cpp
    userliterals.h
    validatingvariantmap.cpp
    validatingvariantmap.h
    variabletreemodel.cpp
    variabletreemodel.h
    vehicleinfomodel.cpp
    vehicleinfomodel.h
    xmlstreamparser.cpp
    xmlstreamparser.h
)

target_include_directories(LmrsCore PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries (LmrsCore PUBLIC Qt6::Network)

add_library(Lmrs::Core ALIAS LmrsCore)
lmrs_doxygen_add_target(LmrsCore)
lmrs_add_translations(LmrsCore)
