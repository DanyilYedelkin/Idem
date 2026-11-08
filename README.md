# Idem

A headless, fixed-timestep simulation core that produces bit-identical results
across compilers, with recording and replay. Work in progress.

## Building

Requires CMake 3.25+ and a C++23 compiler (GCC 14, Clang 19 or Visual Studio 2022).

```sh
cmake --preset release
cmake --build --preset release
ctest --preset release
```

On Windows use the `vs2022` configure preset and the `vs2022-release` build and test presets.
