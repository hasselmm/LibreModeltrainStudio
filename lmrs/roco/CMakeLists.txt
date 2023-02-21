add_library (
    LmrsRoco STATIC
    z21appfileformats.cpp
    z21appfileformats.h
    z21appfilesharing.cpp
    z21appfilesharing.h
    z21client.cpp
    z21client.h
    z21device.cpp
    z21device.h
)

target_include_directories(LmrsRoco PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(LmrsRoco PUBLIC Lmrs::Core Qt6::GuiPrivate Qt6::Network Qt6::Sql)

add_library(Lmrs::Roco ALIAS LmrsRoco)
lmrs_doxygen_add_target(LmrsRoco)
lmrs_add_translations(LmrsRoco)
