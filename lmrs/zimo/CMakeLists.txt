add_library (
    LmrsZimo STATIC
    mx1device.cpp
    mx1device.h
    mx1message.cpp
    mx1message.h
)

target_include_directories(LmrsZimo PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(LmrsZimo PUBLIC Lmrs::Serial)

add_library(Lmrs::Zimo ALIAS LmrsZimo)
lmrs_doxygen_add_target(LmrsZimo)
lmrs_add_translations(LmrsZimo)
