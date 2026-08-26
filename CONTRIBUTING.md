# Contributing

This project uses CMake as the build environment. Minimum required is 3.16 with highest policy set
to 4.4. clang with LLVM is the recommended compiler used for the project.

## Supported Platforms

FileMan supports every platform and toolchain that
[Qt supports](https://doc.qt.io/qt-6.8/supported-platforms.html), owing to extensive use of
Qt-idiomatic codes. However, FileMan hasn't been extensively tested against all platforms and
architectures yet. The following are the minimum working versions for FileMan to be source
compatible and compilable:

- **Architectures:** x86_64, arm64
- **Qt:** >= 6.8
- **C++ Standard:** >= 17
- **CMake:** >= 3.16

#### Linux

- **glib:** 2.8.3
- **pthread:** 2.3.5
- **GCC:** 9

#### Windows

- **MSVC:** 2022 (Windows 10 & 11 only)
- **Mingw-w64:** 13.1

> [!NOTE]
>
> Cross-compiling for Windows from Linux on the same architecture is supported

## Available Build Flags

These flags control various behaviors of the build configuration. Prepend the following
options/flags with `-D` when configuring cmake.

| Flag                   |   Defaults   |  Available Values  | Description                                                                                                                                                                                  |
| :--------------------- | :----------: | :----------------: | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `FM_BUILD_AS`          |   `SHARED`   | `STATIC`, `SHARED` | Sets the build type of the library. If not set, the default is to build shared libraries.                                                                                                    |
| `FM_BUILD_EXAMPLES`    |    `OFF`     |    `ON`, `OFF`     | Enable building example binaries in-tree along with the project. If not set, example binaries are skipped.                                                                                   |
| `BUILD_SHARED_LIBS`    |  undefined   |    `ON`, `OFF`     | Compatibility alternative to `FM_BUILD_AS`. Controls library build type to be either shared (`ON`) or static (`OFF`). Undefined by default, which implies `FM_BUILD_AS=SHARED`               |
| `FM_BUILD_TESTS`       |    `OFF`     |    `ON`, `OFF`     | Enable building project tests along with the project. Testing can be done using CTest. If not set, tests building are skipped. Coupled with `BUILD_TESTING` only if this is the root project |
| `BUILD_TESTING`        |    `OFF`     |    `ON`, `OFF`     | Compatibility alias to `FM_BUILD_TESTS`, only if this is the root project. Ignored otherwise.                                                                                                |
| `CMAKE_INSTALL_PREFIX` | `/usr/local` | \<any valid path\> | Configures prefix directory to install under. Recommended to be one from `$PATH` variable. Some systems may default to `/usr`. For CMake >= 3.23, `cmake --prefix <path>` can also be used.  |

Repository structure follows flat layout as recommended by
[`qt_add_qml_module()`](https://doc.qt.io/qt-6/qt-add-qml-module.html#target-structure). In-source
building is the preferred way, although out-of-source building is possible.

## Building

### 1. Using Builder Image

The [repository registry](https://ghcr.io/lotconn/fileman/builder?tab=tags) hosts an x86_64 builder
image for development tasks tailored for FileMan.

> [!NOTE]
>
> Libraries built within the dev-container are linked against `glibc-2.39`, `binutils-2.41` &
> `glib2-2.80.4` using compilers `gcc-14` or `clang-21`. Make sure your implementation supports
> newer or same versions of the toolchain.

First, from project root, create a container with the builder image and enter :

```bash
podman run --rm -it -v $(pwd):/src:z ghcr.io/lotconn/fileman/builder:latest
```

If using docker:

```bash
docker run --rm -it -v $(pwd):/src:z,shared ghcr.io/lotconn/fileman/builder:latest
```

<details><summary>Further developer notes: Click to expand</summary>

---

To build the image yourself, from project root do:

```bash
podman build -t localhost/fileman-builder:latest images/

# replace with docker if using docker
```

Then create the container and enter:

```bash
podman run --rm -it -v $(pwd):/src:z localhost/fileman-builder
```

If using docker:

```bash
docker run --rm -it -v $(pwd):/src:z,shared localhost/fileman-builder
```

</details>

### 2. Setting up host environment

If you want to set up the environment on your own or integrate with your own project, building
FileMan requires

- `qt6-qtcore-devel`
- `qt6-qtdeclarative-devel`
- `ccache`
- `cmake`
- `clang`, `lld`, `llvm` _OR_ `gcc`, `gcc-c++`
- `nasm`
- `ninja-build`.

> [!Note]
>
> Package names here are for Fedora Linux/EL distributions and their derivatives. Refer to your
> distribution for exact package names

---

Choose one, and after setting up the environment execute:

```bash
cmake -S . -B build/ --fresh -GNinja && cmake --build build/ --parallel
```

The built libraries can then be installed to the default prefix

```bash
# append --prefix path/to/prefix for custom location
cmake --install build/
```

Separate component-wise installation is supported too. Two components are available: `runtime` &
`devel`. To install, for example, the development-related files only, specify `--component devel`
with the previous invocation:

```bash
cmake --install build/ --component devel
```

The repository contains example apps that can be opted in for building during configuration with
`-DFM_BUILD_EXAMPLES=ON`. The built binaries will be available under their respective folders
`examples/<program name>` and will not be exported or installed. They can also be built standalone
without the need for configuring the root project provided the library has already been installed to
running system and available in searchable paths. After ensuring the requirements, change working
directory to the `examples/` folder, configure and build the binary:

```bash
cd examples/
cmake -S . -B build/ --fresh -GNinja && cmake --build build/ --parallel
```

The built binary should be available under `build/`. Try executing the binary:

```bash
QT_LOGGING_RULES="*.debug=true;qt.*.debug=false;qt.qml.import=true" build/<app>
```

The `QT_LOGGING_RULES` environment variable helps to see detailed logs and useful debugging
information. If it's still too verbose for the purpose, omit `qt.qml.import=true` from the line. For
executing outside the container where FileMan isn't installed, you may need to modify
`LD_LIBRARY_PATH` &
[`QML_IMPORT_PATH`](https://doc.qt.io/qt-6/qtqml-syntax-imports.html#qml-import-path) environment
variables pointing to the appropriate directory holding the FileMan library and the QML module. If
built statically however, this is not necessary.

## Testing

To test FileMan with CTest, add `-DFM_BUILD_TESTS=ON` flag during cmake configure:

```bash
cmake -S . -B build/ --fresh -GNinja -DFM_BUILD_TESTS=ON
```

Execute CTest with appropriate build directory:

```bash
ctest --test-dir build/ --output-on-failure
```

Testing is still immature in this project. Subsequent test runs need to be manually preceeded by
these app-directory removals in order for them to be successful:

- `<AppDataLocation>/ModuleTest`
- `<AppDataLocation>/apiTest`
- `<AppDataLocation>/libTest`
- `<AppDataLocation>/RootCtxTest`

On Linux, this can be conveniently done with a simple command:

```bash
rm -rfv \
  ~/.local/share/{Module,api,lib,RootCtx}Test \
  # if using clang-cl/MSVC on wine
  ~/.wine/drive_c/users/admin/AppData/Roaming/{Module,api,lib,RootCtx}Test
```

## Guidelines

Always use the up-to-date _main_ branch as the base. Merge/Pull requests are not accepted if it
can't be automatically merged. Please ensure your MR/PR is up-to-date and rebased on `main` latest
before finalizing drafts.

Try providing a summary highlight of your MR/PR without going into too much details. Multiple
commits on an opened MR/PR-ed branch is allowed, but need to be frozen, validated and pass all tests
before final merge/pull.

## AI Contribution Policy

We aim to be transparent about contributions from AI agents, LLMs, and bots. AI-assisted
contributions by humans are welcome. Any other autonomous AI contributions — including merge/pull
requests, issues, and comments — will be rejected. This includes any "Signed-off-by:
`<agent's operator>`" variants, i.e. human-operated agentic submissions. Human contributors are may
include an "Assisted by: " footer in any contribution that involved AI or LLM assistance but are not
required to.
