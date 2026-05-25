This file contains a brief lists of notable changes for each version. Refer to respective commit messages for detailed descriptions.

# v0.2.0

Reconfigure FileMan to additionally be as a QML module: Part 1

## Bugs
- Fix file can't be added for any category

## Improvments
- Rename fileman.h to .hpp
- Add a top-level `CMakeLists.txt` conforming to QML layout. Notable aspects include:
    - C++17 & Qt>=6.9 Standardized
    - Use as a QML module with plugin support when built as shared (`-DFM_BUILD_AS=SHARED`; already the default)
    - Also can be used as a statically-linked library from C++ implemenatations (`-DFM_BUILD_AS=STATIC`)
    - LTO and RELRO enabled, moderately hardened building
    - clang+llvm is the preferred building tool. CC=clang and CXX=clang++ needs to be specified
    - Build examples in-tree with `-DFM_BUILD_EXAMPLES=ON`. Default is turned off.
    - Defaults to "Release" build if none specified during CMake configuration. Available are: Debug, MinSizeRel, Release & RelWithDebInfo

- Replace custom ENABLE_BITWISE_OPS_FOR_ENUM with Q_DECLARE_OPERATORS_FOR_FLAGS macro, remove unused `scoped-enum-bitops.h` as a result 
- Few code touch-ups
- Replace else if() with if() after returns
- rmFile(): fail if provided category is invalid
- Leverage Q_ASSERT_X for scoped lifetime and avoid race conditions
- Operate on flags scoped on enums
- Move image builder spec files under images/
- Add few necessary packages in the image
- Refine build command, prefer out-of-source building
- Replace with non-login shell as entrypoint

## New
- Add an "All" category alias to select all available categories
- Make FileMan a singleton

Full changelog: [`v0.2.0...v0.1.0`](https://gitlab.com/hasanesa/fileman/-/compare/v0.1.0...v0.2.0)

# v0.1.0

Initial commit

Tree at this point: [`v0.1.0`](https://gitlab.com/hasanesa/fileman/-/tree/v0.1.0)
