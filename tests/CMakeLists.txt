include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE
    SYSTEM)
FetchContent_MakeAvailable(Catch2)

add_executable(idem_tests
    broadphase_tests.cpp
    determinism_tests.cpp
    math_tests.cpp
    recording_tests.cpp
    rng_tests.cpp
    scene_tests.cpp
    simulation_tests.cpp
)
target_link_libraries(idem_tests PRIVATE idem::idem Catch2::Catch2WithMain)
idem_target_warnings(idem_tests)

include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
catch_discover_tests(idem_tests)
