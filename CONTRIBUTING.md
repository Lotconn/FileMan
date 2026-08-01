## Contributing

This project uses CMake as the build environment. Minimum required is 3.16 with highest policy set to 4.4. clang with LLVM is the recommended compiler used for the project.

---

### Available Build Flags:

These flags control various behaviors of the build configuration. Prepend the following options/flags with `-D` when configuring cmake.

| Flag                   |   Defaults   |  Available Values  | Description                                                                                                                                                                                 |
| :--------------------- | :----------: | :----------------: | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `FM_BUILD_AS`          |   `SHARED`   | `STATIC`, `SHARED` | Sets the build type of the library. If not set, the default is to build shared libraries.                                                                                                   |
| `FM_BUILD_EXAMPLES`    |    `OFF`     |    `ON`, `OFF`     | Enable building example binaries in-tree along with the project. If not set, example binaries are skipped.                                                                                  |
| `BUILD_SHARED_LIBS`    |  undefined   |    `ON`, `OFF`     | Compatibility alternative to `FM_BUILD_AS`. Controls library build type to be either shared (`ON`) or static (`OFF`). Undefined by default, which implies `FM_BUILD_AS=SHARED`              |
| `CMAKE_INSTALL_PREFIX` | `/usr/local` | \<any valid path\> | Configures prefix directory to install under. Recommended to be one from `$PATH` variable. Some systems may default to `/usr`. For CMake >= 3.23, `cmake --prefix <path>` can also be used. |

Repository structure follows flat layout as recommended by [`qt_add_qml_module()`](https://doc.qt.io/qt-6/qt-add-qml-module.html#target-structure). In-source building is the preferred way, although out-of-source building is possible but issues a warning about QML module discovery. The latter can be approached for development builds.

---

### Building

The [repository registry](https://gitlab.com/lotconn/fileman/container_registry) hosts builder images for development tasks tailored for FileMan.

First, from project root, create a container with the builder image and enter :

```bash
podman run --rm -it -v $(pwd):/src:z registry.gitlab.com/lotconn/fileman/builder:latest
```

If using docker:

```bash
docker run --rm -it -v $(pwd):/src:z,shared registry.gitlab.com/lotconn/fileman/builder:latest
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

---

</details>

From there execute:

```bash
CC=clang CXX=clang++ cmake -B . --fresh -GNinja && cmake --build . --parallel
```

Alternatively, just press up arrow key and hit enter! The built libraries can then be installed to the default prefix

```bash
# append --prefix path/to/prefix for custom location
cmake --install .
```

Separate component-wise installation is supported too. Two components are available: `runtime` & `devel`. To install, for example, the development-related files only, specify `--component devel` with the previous invocation:

```bash
cmake --install . --component devel
```

The repository contains example apps that can be opted in for building during configuration with `-DFM_BUILD_EXAMPLES=ON`. The built binaries will be available under their respective folders `examples/<program name>` and will not be exported or installed. They can also be built standalone without the need for configuring the root project provided the library has already been installed to running system and available in searchable paths. This however, may require a manual change of include directive in `main.cpp` files from `#include <fileman>` to `#include <Lotconn/FileMan/fileman>`. After ensuring the requirements, change working directory to the `examples/` folder, configure and build the binary:

```bash
cd examples/
CC=clang CXX=clang++ cmake -S . -B build/ --fresh -GNinja && cmake --build build/ --parallel
```

The built binary should be available under `build/`. Try executing the binary:

```bash
QT_LOGGING_RULES="*.debug=true;qt.*.debug=false;qt.qml.import=true" build/<app>
```

The `QT_LOGGING_RULES` environment variable helps to see detailed logs and useful debugging information. If it's still too verbose for the purpose, omit `qt.qml.import=true` from the line. For executing outside the container where FileMan isn't installed, you may need to modify `LD_LIBRARY_PATH` & [`QML_IMPORT_PATH`](https://doc.qt.io/qt-6/qtqml-syntax-imports.html#qml-import-path) environment variables pointing to the appropriate directory holding the FileMan library and the QML module. If built statically however, this is not necessary.

---

If you want to set up the environment on your own or integrate with your own project, building FileMan requires `qt6-qtcore-devel`, `qt6-qtdeclarative-devel`, `ccache`, `cmake`, `clang`, `lld`, `llvm`, `nasm` & `ninja-build`.

> [!Note]
> Package names here are for Fedora Linux/EL distributions and their derivatives. Refer to your distribution for exact package names

For Qt6/C++ implementations, adjust your CMakeLists file to contain these command invocations in the least:

```CMakeLists
cmake_minimum_required(VERSION 3.16)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(FileMan REQUIRED)

qt_standard_project_setup()

target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Core Lotconn::FileMan)

# lld is required for clang, can be omitted if using gcc instead
target_link_options(${PROJECT_NAME} PRIVATE $<$<CXX_COMPILER_ID:Clang>:-fuse-ld=lld>)
```

---

### Guidelines

Always use the up-to-date _main_ branch as the base. Merge/Pull requests are not accepted if it can't be automatically merged. Please ensure your MR/PR is up-to-date and rebased on `main` latest before finalizing drafts.

Try providing a summary highlight of your MR/PR without going into too much details. Multiple commits on an opened MR/PR-ed branch is allowed, but need to be frozen, validated and pass all tests before final merge/pull.

---

### AI Contribution Policy

We aim to be transparent about contributions from AI agents, LLMs, and bots. AI-assisted contributions by humans are welcome. Any other autonomous AI contributions — including merge/pull requests, issues, and comments — will be rejected. This includes any "Signed-off-by: `<agent's operator>`" variants, i.e. human-operated agentic submissions. Human contributors are may include an "Assisted by: " footer in any contribution that involved AI or LLM assistance but are not required to.

---
