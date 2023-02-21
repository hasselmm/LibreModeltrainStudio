function(lmrs_add_test FILENAME)
    get_filename_component(TESTNAME "${FILENAME}" NAME_WE)
    add_executable(${TESTNAME} ${FILENAME})

    foreach(extension IN ITEMS .cpp .h .qrc)
        if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${TESTNAME}${extension}")
            target_sources(${TESTNAME} PRIVATE "${TESTNAME}${extension}")
        endif()
    endforeach()

    target_link_libraries(${TESTNAME} PUBLIC Qt6::Test ${ARGN})
    add_test(NAME ${TESTNAME} COMMAND $<TARGET_FILE:${TESTNAME}>)
endfunction()


lmrs_add_test(tst_algorithms.cpp Lmrs::Core)
lmrs_add_test(tst_continuation.cpp Lmrs::Core)
lmrs_add_test(tst_dccconstants.cpp Lmrs::Core)
lmrs_add_test(tst_dccrequest.cpp Lmrs::Core)
lmrs_add_test(tst_lp2message.cpp Lmrs::Esu)
lmrs_add_test(tst_lp2stream.cpp Lmrs::Esu)
lmrs_add_test(tst_propertyguard.cpp Lmrs::Core)
lmrs_add_test(tst_speeddial.cpp Lmrs::Widgets)
lmrs_add_test(tst_variablecontrol.cpp Lmrs::Core)
lmrs_add_test(tst_z21client.cpp Lmrs::Roco)

set_target_properties(
    tst_dccconstants PROPERTIES
    FOLDER "qtc_runnable"
)

