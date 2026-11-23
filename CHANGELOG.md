# CHANGELOG

<!-- markdownlint-disable no-duplicate-heading -->

This file contains a brief lists of notable changes for each version. Refer to respective commit
messages for detailed descriptions.

---

## v3.0.0

### Breaking Changes

- Now using Qt 6.12 LTS, CMake 3.24, and C++20 as the standard and baseline project-wide
- Router header renamed to `FileMan`; includes now need to use as `<FileMan>`
- Removed `create()` as QmlEngine was never calling it owing to the availability of the default
  constructor
- Dropped explicit enablement of position independent code in build. Build system however, may still
  enable it by default but this isn't enforced anymore now.
- `rm_file_t::status_h()` renamed to `status_b()`
- `FileMan::Errno` renamed to `FileMan::Error`
- `FileMan::Error::Cancelled` renamed to `FileMan::Error::NotADir`. Any `ECANCELLED` errno return is
  also replaced with `ENOTDIR`.
- The root store dir moved inside one-level deeper `<AppLocalDataDir>/store` versus previously
  `<AppLocalDataDir>/`. Directory structure within the root is retained.
- `getDataDir()` renamed to `storeDir()`. Attributes and behavior otherwise are retained.
- `categoryToString()` renamed to `toString()` and made static with another overload introduced for
  `FileMan::Error`. Behavior otherwise is retained.
- `getFileName()` renamed to `fileName()`. Attributes and behavior otherwise are retained.
- `getCategoryDir()` renamed to `categoryDir()`. Attributes and behavior otherwise are retained.

### New

- Add some tests for the library, utlizing CTest. Testing is still immature and will develop through
  the project's lifetime. Testing can be enabled using the flags `FM_BUILD_TESTS` for
  project-specific testing or the global `BUILD_TESTING`.
- `addFile()` method now takes an `overwrite` parameter to specify if existing files should be
  overwritten on new addition or not. The old syntax and behavior is still available as an overload
  of this new method.
- `addFile()`'s return type `add_file_t` now carries an additional member providing the ID of the
  added file. The corresponding data can be accessed using `id()` method in the type.
- Introduced three new `FileMan` class constructors each controlling application store directory in
  different ways.
- Added `instanceExists()` to check singleton instance existence
- Added `FileMan::Error::NotUnique` representing `ENOTUNIQ` (value 76). Would usually be returned if
  blob is linked with multiple symlinks in the store.
- `fromStringToCategory()` and `fromStringToError()` added to translate string name to enum. Invalid
  string names return `Category::None` on the former and `Error::Success` on the latter.
- `rmFile(const QString &fileId)` overload allowing direct and more complete removal of blobs and
  all its associated links using the `id()` as provided by `addFile()`

### Improvements

- Multiline CMake generator expressions are moved to conditional blocks since this isn't a defined
  behavior and may not work in future.
- `Private` class got renamed to `FileManPrivate` along with some other refactors to simplify the
  design. Also follows Qt idiomatic style.
- Added some branch prediction optimization
- Blobs having multiple symlinks pointing to it are now not flagged as inappropriate in
  `rmFile(const QUrl &)` instead of removing them blindly. This reduces chances of invalidating any
  other symlinks in the store. `refreshStore()` however keeps only the oldest symlink entry among
  the group. Facing such situation in real world is very much unlikely from first, unless in some
  filesystem error edge cases.

### Fixes

- `BUILD_SHARED_LIBS=OFF` is now correctly respected
- Referencing `add_file_t` and `rm_file_t` after ownership transfer doesn't crash anymore
- C++ standard is not enforced anymore when FileMan is included in downstream projects
- Files in the application store can't be externally modified anymore; files are now reflinked
  opportunistically on filesystems that support it rather than hardlinked. On supported filesystems,
  the space increase for a reflinked file may range somewhere between 2-10% of the original file
  size, providing excellent space-efficiency proximity of hardlinking and platform agnostic way to
  mitigate the remote crafting security bug through a vector.
- Dangling symlinks in the store are now removed on `refreshStore()` call
- Symlinks residing in the wrong category directory are also correctly cleaned by `refreshStore()`.
- Files from store are now correctly and explicitly denied from adding

### Others

- `internal-helpers` have been removed. All methods there have moved to `FileManPrivate` class as
  static members.
- All private methods and data structures in `FileMan` class also got moved to `FileManPrivate`
- Auto-formatting and linting enabled for almost all file-types used in the project. Relevant
  styling configurations are included in the tree.
- Added `logFatal` for replacing `qFatal()` loggings. The logging prefix strings were also modified
  to improve readability and visuals
- Instances of "hash" wording replaced with "blob" as appropriate. The former is preserved for
  specifying checksum hashes only. This consequentially renamed some internal methods.
- Also, C++ follows Qt's code style. Refer to <https://wiki.qt.io/Qt_Coding_Style> and
  <https://wiki.qt.io/Qt_Coding_Style>.
- Workspace configurations and extensions recommendations were added for proper VSCode/VSCodium
  environment setup
- Doxygen inline briefs were added in private methods for which documentations are not written
- Documentation updates and fixes for new version, new and changed methods

Full changelog: [`v2.0.2...v3.0.0`](https://gitlab.com/lotconn/fileman/-/compare/v2.0.2...v3.0.0)

---

## v2.0.2

This release resolves a major bug affecting library lookup from the Qml plugin and unguarded CF flag
on MSVC

### Fixes

- Set appropriate rpath for Qml modules rather than the default
  `$ORIGIN:$ORIGIN/../${CMAKE_INSTALL_LIBDIR}`
- Set control flow guard on MSVC for x86_64/amd64 architectures only

#### Others

- README: `<fileman>` should be used over namespace-prefixed `<Lotconn/FileMan/fileman>`

Full changelog: [`v2.0.1...v2.0.2`](https://gitlab.com/lotconn/fileman/-/compare/v2.0.1...v2.0.2)

---

## v2.0.1

This is a bug-fix release focusing on Windows (MSVC) build compatibility, logic bug fixes, and
documentation/build system improvements.

### Fixes

- Store folder permissions aren't fixed on Windows.
- Store directory self-healing failing due to incorrect `QDir(dir).remove(dirName)` usage
- Dir checks incorrectly passing if a category path is a regular file
- `TypeError` on returns of QML example apps
- Symlinks resolving relative targets against the app's CWD on Windows
- MSVC build errors: unresolved external symbols
- MSVC build error `C3246` (cannot inherit from 'FileMan' as it has been declared as 'final')
- Undefined macro `Q_DISABLE_COPY_MOVE_X` on Qt <= 6.8
- CMake IPO error on unsupported platforms
- Reference links not working in README.md

### Improvements

- Add MSVC-equivalent hardening flags (`/Oy-`, `/EHsc`, `/GS`, `/W3`, `/guard:cf`) matching
  GCC/Clang options
- Exclude MSVC from GCC/Clang-specific compile & link options
- Make `WINDOWS_EXPORT_ALL_SYMBOLS` target-specific and remove it in favor of explicit
  dllimport/export attributes
- Drop backward compatibility across major versions in CMake package config (`SameMajorVersion`)
- Declare LTO/IPO opportunistically using `CheckIPOSupported`
- Rephrase README about reentrancy and sub-classing
- Auto-format markdown and QML files, add `.prettierrc` and QML formatter configs
- Use `const` and `let` broadly over `var` in QML example apps
- Add useful debug logging for pruning stale cache and dir checks
- Explicitly use `this->` for `d->` pointer access

### New

- Globally set `CMAKE_POSITION_INDEPENDENT_CODE`
- Add "Supported Platforms" section in README and CONTRIBUTING
- Add template CMake toolchain files for linux-native and windows-cross-compile
- Add "Refresh Store" button in example QML apps
- Register `MIGRATING.md` for installation

### Misc

- Update container registry location to `ghcr.io/lotconn/fileman/builder`
- Bundle custom Qt 6.8 installation inside the builder container for better project compliance
- Stop annoying CMake reconfigures on edits or workspace loads in VSCode settings
- Fix typos in CONTRIBUTING.md

Full changelog: [`v2.0.0...v2.0.1`](https://gitlab.com/lotconn/fileman/-/compare/v2.0.0...v2.0.1)

---

## v2.0.0

ABI+API breaking release. Introduces an instance-based improved singleton design with
PIMPL/d-pointer for ensuring ABI stability, opaque `add_file_t`/`rm_file_t` return-type structs,
in-memory store-state caching, cross-platform hardlink deduplication, and various robustness,
security & QML integration improvements. Ownership transferred from individual maintainer to the
`Lotconn` organization.

### Breaking Changes

- Organization & namespace migrated from `HasanEsa::` to `Lotconn::`: CMake target/install
  namespace, QML module URI (`Lotconn.FileMan`), plugin class (`Lotconn_FileManPlugin`), and
  installed header path (`Lotconn/FileMan/fileman`) all changed accordingly; copyright in `LICENSE`
  transferred to `Lotconn`
- `addFile()` now returns an ABI-safe `add_file_t` struct (`status()`, `location()`) instead of a
  `QVariantMap`
- `rmFile()` now takes a fully-qualified `const QUrl &fileUrl` instead of `QString fileName` +
  `Categories category`, and returns an ABI-safe `rm_file_t` struct (`status_l()`, `status_h()`)
  instead of a plain `int`
- `storeHouseKeeping()` renamed to `refreshStore()`
- `FileMan` is now an instance-based singleton and marked `final`. Explicit instantiation is
  required before touching any member. Constructing a second instance, or instantiating before
  `QCoreApplication::applicationName()` is set, now fatally aborts the process
- `getDataDir()` moved from a `static` member to an instance `const` member; conversely
  `getFileName()` moved from an instance member to a `static` member. Use the new
  `FileMan::instance()` to reach instance members externally without holding a reference
- `addFile()`, `rmFile()` & `refreshStore()` no longer carry `[[nodiscard]]`
- `Category` enum's underlying type changed from `unsigned int` to `quint8`; `FileMan::None = 0x00`
  was added for internal use
- Dropped unneeded `Q_ENUM(Category)`

### New

- `FileMan::instance()` static accessor for referring to an already constructed instance without
  holding a reference
- `categoryToString()` function for translating FileMan::Category enum to parsable string
- `getCategoryDir(Category)` returns the absolute path of a category's directory in store
- `FileMan::Errno` enum maps common `errno` values to QML-friendly comparators
- Allow `<fileman>` to be used along with conventional `<fileman.hpp>`
- Cross-platform hardlink-based deduplication, including Windows (previously UNIX-only)
- In-memory O(1) store-state cache (`hashToLinkDict`/`linkToHashDict`) bypassing most filesystem IO
  on `addFile()`/`rmFile()`
- Exclusive on-disk (`QLockFile`) + in-process (`QMutex`) locking around store mutations in
  `addFile()`/`rmFile()`/`refreshStore()`, auto-released via `QScopeGuard`
- Self-healing of orphaned blobs and stale dictionary entries during
  `addFile()`/`revLookupSymlink()`/`refreshStore()`
- Single-pass file hashing + copy-staging, avoiding a second redundant full read of the source file
- `CONTRIBUTING.md` split out of `README.md` into its own file, and installed alongside other docs
- `.clangd`, `.gitignore` & `.vscode/settings.json` project config files
- `AI Contribution Policy` section updated in `CONTRIBUTING.md`

### Improvements

- `addFile()`/`rmFile()`/`refreshStore()` validate the on-disk store before attempting operations,
  failing fast on an anomalous store instead of handling scattered errors later
- Duplicate-file handling in `addFile()` is unambiguous: the existing symlink is returned if
  present, or the orphaned blob is reused if not linked to any symlink, always paired with `EEXIST`
- `addFile()` uses `QFileInfo::absoluteFilePath()` instead of `canonicalFilePath()`, avoiding
  implicit symlink resolution ahead of validation
- Category directories are (re)created directly from `refreshStore()`'s validation/repair pass
- `preAddValidate()` returns the combined eligible-category bitmask for a file via a pre-populated
  lookup dictionary instead of matching against a single supplied category
- `preRmValidate()` validates a full absolute file path directly (with the optional enforced-check
  toggle retained as previous) instead of matching a bare file name under a category directory
- Wider usage of `""_L1` string literals over `QLatin1StringView()`
- Define more explicitly than `auto` where reasonable
- `CMakeLists.txt`: highest supported CMake policy raised to 4.4; Qt baseline
  (`qt_standard_project_setup()`/`QT_ENABLE_STRICT_MODE_UP_TO`) pinned to the last LTS (6.8) instead
  of 6.9; now tracks latest Qt LTS releases
- Intel CET (`-fcf-protection`) now only enabled on x86_64/amd64
- `-Wall -Wextra` added to catch unnoticed flaws cascading to hard-to-track bugs
- Monolithic `fileman.cpp` split into one file per public method (`addFile.cpp`,
  `categoryToString.cpp`, `getCategoryDir.cpp`, `getDataDir.cpp`, `getFileName.cpp`,
  `refreshStore.cpp`, `rmFile.cpp`), plus dedicated `internal-helpers.{hpp,cpp}` and PIMPL
  `fileman_p.{hpp,cpp}`
- `alignas(n)` used in place of GCC/Clang-specific `__attribute__((aligned(n)))` for MSVC
  portability
- `README.md`/`CONTRIBUTING.md` documentation overhaul: accurate cross-platform data-dir
  description, new "Performance, Security & Reliability", "The Singleton Design", "Thread safety",
  "Reentrancy", "Stability" & "RESTful API conformity" sections, and a CMake `FetchContent`
  integration method
- Container builder image bumped to `rockylinux/rockylinux:10.2-ubi`, marks `/src` as a Git safe
  directory

### Fixes

- Add the missing code to gate invalid categories passed to `addFile()` & `rmFile()`
- [Security] `rmFile()` no longer matches by bare file name against a categorized directory; the
  resolved absolute path is validated against the expected category directory before removal,
  closing a path-confinement/arbitrary-deletion vector
- Example apps: missing singleton initialization, faulty category selection in the CLI removal
  option, and a `std::array` → `int*` conversion error

### Others

- `CLAUDE.md` added, documenting AI-agent contribution directives

Full changelog: [`v1.0.1...v2.0.0`](https://gitlab.com/lotconn/fileman/-/compare/v1.0.1...v2.0.0)

---

## v1.0.1

This is a minor bug-fix release, mostly concerning CMake build system

### Bug fixes

- Do not propagate Qt API strictness to consumers
- Scope build flags for GCC & clang only
- Scope `lld` linker to clang conditionally, gcc doesn't need explicit linker override anymore

### Improvements

- Remove redundant compile options already handled by cmake
- Move in-tree example building to the very last stage of the build process
- Split development and runtime related components to separate `devel` & `runtime` components that
  can be separately installed
- Do not install documentations and license files when used as a submodule
- Documentation improvements, fix typos and code snippets, reformat for better readability
- Rename the example apps `fileman-cpp` as `fileman-cli-qt` & `fileman-app` as `fileman-qml-legacy`

### New

- `FM_BUILD_AS` option can also be implicitly set with more common and general `BUILD_SHARED_LIBS`
  but not vice-versa
- Add a section about using the system-installed library

### Others

Full changelog: [`v1.0.0...v1.0.1`](https://gitlab.com/lotconn/fileman/-/compare/v1.0.0...v1.0.1)

---

## v1.0.0

FileMan can now be used as a QML module with QML apps. This allows convenient integration over the
legacy approach of setting context properties from C++ backend. Refer to README.md for the details.

### Bug fixes

- Examples are built as non-\<BUILD_TYPE\>. Now they default to "Release" as expected if nothing is
  specified
- Keep plugin name as (lib)FileManplugin. Renaming breaks locating it correctly

### Improvements

- Add an example app demonstrating QML module implementation of FileMan. This is the modern and
  preferred way of integrating in QML apps. This is same as fileman-app but the implementation
  differs. As with other example apps, this too can be built both in-tree with the root project, and
  built standalone provided FileMan is installed to system. Refer to README.md for details. The key
  parts of the codes here are highlighted with relevant comments too.
- Create & install the plugin even on static building. Contrary to previous decisions, this is
  required for downstream integration

### New

- Export the plugin so that it is available for linking against

### Others

- Remove redundant Qt policy settings, move the details to comments instead
- Add details about disabled APIs on Qt's version strict mode in the comments

Full changelog: [`v0.2.1...v1.0.0`](https://gitlab.com/lotconn/fileman/-/compare/v0.2.1...v1.0.0)

---

## v0.2.1

This is a patch version to fix and lightly edit few leftover bugs from previous versions, add a GUI
app example and produce versioned libraries

### Improvements

- Libraries are versioned semantically with symlink chains

### Others

- Make logs in rmFile() more comprehensible
- Add two example implementations of the FileMan library. One of these is a Qt/C++ cli app and
  another is a GUI QML app following legacy ways of setting context properties
- Bump version
- Produce versioned libraries with symlink chains
- Tell about lld in the CMakeLists snippet of README.md
- Add a license/copyright header and intro about the app on the examples codes
- Highlight key parts on the examples code related to FileMan integration

Full changelog: [`v0.2.0...v0.2.1`](https://gitlab.com/lotconn/fileman/-/compare/v0.2.0...v0.2.1)

---

## v0.2.0

This version fixes some critical bugs, adds a catch-all "All" category on the APIs and lays out good
ground work for FileMan to be a reusable library in good sense; the library is now a singleton, is
installable, can be used a shared library or be linked statically with the implementing application
and can also be used as a QML module. The QML module part is still in progress and not guarranteed
to be stable.

### Bugs

- Fix file can't be added for any category
- Fix storage info always fails during adding

### Improvments

- Rename fileman.h to .hpp
- Add a top-level `CMakeLists.txt` conforming to QML layout. Notable aspects include:
  - C++17 & Qt>=6.9 Standardized
  - Use as a QML module with plugin support when built as shared (`-DFM_BUILD_AS=SHARED`; already
    the default)
  - Also can be used as a statically-linked library from C++ implemenatations
    (`-DFM_BUILD_AS=STATIC`)
  - LTO and RELRO enabled, moderately hardened building
  - clang+llvm is the preferred building tool. CC=clang and CXX=clang++ needs to be specified
  - Build examples in-tree with `-DFM_BUILD_EXAMPLES=ON`. Default is turned off.
  - Defaults to "Release" build if none specified during CMake configuration. Available are: Debug,
    MinSizeRel, Release & RelWithDebInfo

- Replace custom ENABLE_BITWISE_OPS_FOR_ENUM with Q_DECLARE_OPERATORS_FOR_FLAGS macro, remove unused
  `scoped-enum-bitops.h` as a result
- Few code touch-ups
- Replace else if() with if() after returns
- rmFile(): fail if provided category is invalid
- Leverage Q_ASSERT_X for scoped lifetime and avoid race conditions
- Operate on flags scoped on enums
- Move image builder spec files under images/
- Add few necessary packages in the image
- Refine build command, prefer out-of-source building
- Replace with non-login shell as entrypoint

### New

- Add an "All" category alias to select all available categories
- Make FileMan a singleton

Full changelog: [`v0.1.0...v0.2.0`](https://gitlab.com/lotconn/fileman/-/compare/v0.1.0...v0.2.0)

---

## v0.1.0

Initial commit

Tree at this point: [`v0.1.0`](https://gitlab.com/lotconn/fileman/-/tree/v0.1.0)
