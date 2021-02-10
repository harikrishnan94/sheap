set(TEST_SRC "${TEST_DIR}/testBase.cpp" "${TEST_DIR}/test_sheap.cpp")

find_package(doctest CONFIG REQUIRED)
find_package(Boost REQUIRED)
find_package(Threads)

add_executable(${TEST} ${TEST_SRC})
target_link_libraries(${TEST} PRIVATE ${LIB} doctest::doctest Boost::boost
                                      ${CMAKE_THREAD_LIBS_INIT})

enable_testing()
add_test(NAME ${TEST} COMMAND ${TEST} -d)
