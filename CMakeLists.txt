cmake_minimum_required(VERSION 3.25)

project(Idem
    VERSION 0.1.0
    DESCRIPTION "Deterministic fixed-step simulation core with replay verification"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(IDEM_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
option(IDEM_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(IDEM_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(IdemCompilerOptions)

add_subdirectory(src)

if(IDEM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
