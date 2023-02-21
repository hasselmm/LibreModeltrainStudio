add_library (
    LmrsKpfZeller STATIC
    scdevice.cpp
    scdevice.h
)

target_include_directories(LmrsKpfZeller PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(LmrsKpfZeller PUBLIC Lmrs::Core Qt6::SerialPort)

add_library(Lmrs::KpfZeller ALIAS LmrsKpfZeller)
lmrs_doxygen_add_target(LmrsKpfZeller)
lmrs_add_translations(LmrsKpfZeller)
