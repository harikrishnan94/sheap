set(BENCH_SRC "${BENCH_DIR}/bench_sheap.cpp")

find_package(benchmark CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread program_options)

add_executable(${BENCH} ${BENCH_SRC})
target_link_libraries(${BENCH}
                      PRIVATE ${LIB}
                              ${CMAKE_THREAD_LIBS_INIT}
                              ${Boost_LIBRARIES}
                              benchmark::benchmark_main)
