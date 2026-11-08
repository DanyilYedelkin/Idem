add_library(idem
    broadphase/brute_force.cpp
    scene.cpp
)
add_library(idem::idem ALIAS idem)

target_include_directories(idem PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)
target_compile_features(idem PUBLIC cxx_std_23)

idem_target_warnings(idem)
idem_target_strict_fp(idem)
