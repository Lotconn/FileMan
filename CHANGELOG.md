This file contains a brief lists of notable changes for each version. Refer to respective commit messages for detailed descriptions.

---

# v1.0.0

FileMan can now be used as a QML module with QML apps. This allows convenient integration over the legacy approach of setting context properties from C++ backend. Refer to README.md for the details.

## Bug fixes
- Examples are built as non-\<BUILD_TYPE\>. Now they default to "Release" as expected if nothing is specified
- Keep plugin name as (lib)FileManplugin. Renaming breaks locating it correctly

## Improvements
- Add an example app demonstrating QML module implementation of FileMan. This is the modern and preferred way of integrating in QML apps. This is same as fileman-app but the implementation differs. As with other example apps, this too can be built both in-tree with the root project, and built standalone provided FileMan is installed to system. Refer to README.md for details. The key parts of the codes here are highlighted with relevant comments too.
- Create & install the plugin even on static building. Contrary to previous decisions, this is required for downstream integration

## New
- Export the plugin so that it is available for linking against

## Others
- Remove redundant Qt policy settings, move the details to comments instead
- Add details about disabled APIs on Qt's version strict mode in the comments

---

# v0.2.1

This is a patch version to fix and lightly edit few leftover bugs from previous versions, add a GUI app example and produce versioned libraries
    
## Improvements
- Libraries are versioned semantically with symlink chains

## Others
- Make logs in rmFile() more comprehensible
- Add two example implementations of the FileMan library. One of these is a Qt/C++ cli app and another is a GUI QML app following legacy ways of setting context properties
- Bump version
- Produce versioned libraries with symlink chains
- Tell about lld in the CMakeLists snippet of README.md
- Add a license/copyright header and intro about the app on the examples codes
- Highlight key parts on the examples code related to FileMan integration

---

# v0.2.0

This version fixes some critical bugs, adds a catch-all "All" category on the APIs and lays out good ground work for FileMan to be a reusable library in good sense; the library is now a singleton, is installable, can be used a shared library or be linked statically with the implementing application and can also be used as a QML module. The QML module part is still in progress and not guarranteed to be stable.

## Bugs
- Fix file can't be added for any category
- Fix storage info always fails during adding

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

---

# v0.1.0

Initial commit

Tree at this point: [`v0.1.0`](https://gitlab.com/hasanesa/fileman/-/tree/v0.1.0)
