set(BENCH_SRC "${BENCH_DIR}/bench_sheap.cpp")

find_package(benchmark CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread program_options)

if(NOT WIN32)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JEMALLOC jemalloc)
endif()

add_executable(${BENCH} ${BENCH_SRC})
target_link_libraries(${BENCH} PRIVATE ${LIB} Boost::boost Boost::thread Boost::program_options
                                       benchmark::benchmark_main)

if(NOT WIN32)
    target_link_libraries(${BENCH} PRIVATE ${LIB} ${JEMALLOC_LINK_LIBRARIES})
endif()
