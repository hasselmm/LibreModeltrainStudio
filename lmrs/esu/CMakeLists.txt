add_library (
    LmrsEsu STATIC
    functionmappingmodel.cpp
    functionmappingmodel.h
    lp2constants.cpp
    lp2constants.h
    lp2device.cpp
    lp2device.h
    lp2message.cpp
    lp2message.h
    lp2request.cpp
    lp2request.h
    lp2response.cpp
    lp2response.h
    lp2stream.cpp
    lp2stream.h
)

target_include_directories(LmrsEsu PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(LmrsEsu PUBLIC Lmrs::Serial)

add_library(Lmrs::Esu ALIAS LmrsEsu)
lmrs_doxygen_add_target(LmrsEsu)
lmrs_add_translations(LmrsEsu)
