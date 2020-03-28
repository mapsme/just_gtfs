file(GLOB TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp)

message(STATUS "CMAKE_CURRENT_BINARY_DIR=" ${CMAKE_CURRENT_BINARY_DIR})

foreach(TEST_SOURCE ${TESTS})
    string(REPLACE ".cpp" "" TEST_TARGET "${TEST_SOURCE}")
    add_executable(${TEST_TARGET} ${TEST_SOURCE})
    target_compile_features(${TEST_TARGET} PRIVATE cxx_std_17)
    add_test("${TEST_TARGET}" "${TEST_TARGET}" WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} --verbose)
endforeach()
