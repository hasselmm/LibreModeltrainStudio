add_library (
    LmrsSerial STATIC
    serialportmodel.cpp
    serialportmodel.h
)

target_include_directories(LmrsSerial PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(LmrsSerial PUBLIC Lmrs::Core Qt6::SerialPort)

add_library(Lmrs::Serial ALIAS LmrsSerial)
lmrs_doxygen_add_target(LmrsSerial)
lmrs_add_translations(LmrsSerial)
