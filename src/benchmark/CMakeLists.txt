set(BENCH_SRC "${BENCH_DIR}/bench_sheap.cpp")

find_package(PkgConfig REQUIRED)
find_package(benchmark CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread program_options)

pkg_check_modules(JEMALLOC jemalloc)

add_executable(${BENCH} ${BENCH_SRC})
target_link_libraries(${BENCH}
                      PRIVATE ${LIB}
                              ${CMAKE_THREAD_LIBS_INIT}
                              ${Boost_LIBRARIES}
                              benchmark::benchmark_main
                              ${JEMALLOC_LINK_LIBRARIES})
