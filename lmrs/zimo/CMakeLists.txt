add_library (
    LmrsZimo STATIC
    mx1device.cpp
    mx1device.h
    mx1message.cpp
    mx1message.h
)

target_include_directories(LmrsZimo PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(LmrsZimo PUBLIC Lmrs::Core Qt6::SerialPort)

add_library(Lmrs::Zimo ALIAS LmrsZimo)
lmrs_add_translations(LmrsZimo)
