include(FetchContent)

set(BENCHMARK_ENABLE_TESTING OFF CACHE INTERNAL "")
set(BENCHMARK_ENABLE_INSTALL OFF CACHE INTERNAL "")
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE INTERNAL "")
set(BENCHMARK_ENABLE_WERROR OFF CACHE INTERNAL "")

FetchContent_Declare(benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.1
    GIT_SHALLOW TRUE
    SYSTEM)
FetchContent_MakeAvailable(benchmark)

add_executable(idem_bench broadphase_bench.cpp)
target_link_libraries(idem_bench PRIVATE idem::idem benchmark::benchmark_main)
idem_target_warnings(idem_bench)
