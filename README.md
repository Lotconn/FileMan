# FileMan

Simple file manager library for Qt/QML apps written in Qt6/C++

---

FileMan provides simple APIs and robust backend for managing data store for Qt/QML applications.

**Note:** FileMan is a singleton, moderately hardened, async-safe, thread-safe and is reentrant.

## FileMan Class

| Use case | Integration |
|----------|-------------|
| Header:  | `#include <HasanEsa/FileMan/fileman.h>` |
| CMake:   | `find_package(FileMan REQUIRED)`<br>`target_link_libraries(mytarget PRIVATE HasanEsa::FileMan)`<br>`target_link_libraries(mytarget PRIVATE HasanEsa::FileManplugin)` \[static building\]<br>`qt_add_qml_module(mytarget DEPENDENCIES HasanEsa.FileMan)` \[QML module\] |
| QML:     | `import HasanEsa.FileMan` |
| CPP:     | `#include <QtQml/QQmlExtensionPlugin>`<br>`Q_IMPORT_QML_PLUGIN(HasanEsa_FileManPlugin)` \[static building\] |


### Public Types
| Type | Description |
|------|-------------|
| enum | [Category](#category-enum) |
| flag | [Categories](#category-enum) |

### Public Functions

| Return type | Function |
|--:|:--|
| [QVariantMap](https://doc.qt.io/qt-6/qvariant.html) | [addFile](#addfile)([QUrl](https://doc.qt.io/qt-6/qurl.html) &fileURL, [FileMan::Categories](#fileman-category) category = FileMan::All) |
| [QString](https://doc.qt.io/qt-6/qstring.html) | [getFileName](#getfilename)([QUrl](https://doc.qt.io/qt-6/qurl.html) &fileURL) |
| int | [rmFile](#rmfile)([QString](https://doc.qt.io/qt-6/qstring.html) fileName, [FileMan::Categories](#fileman-category) category = FileMan::All) |
| int | [storeHouseKeeping()](#storehousekeeping) |

### Static Public Members

| Return type | Function |
|--:|:--|
| [QString](https://doc.qt.io/qt-6/qstring.html) | getDataDir() |

***

## Detailed Description
FileMan is a file manager backend for managing application store. It creates and manages the application store under `$XDG_DATA_HOME/<appName>/`. FileMan matches the MIME type of the file against the specified [category](#category-enum) during adding, cryptographically hashes it (currently using SHA256) and stores the raw "blob" with the digest name under `$XDG_DATA_HOME/<appName>/common`. A symlink to this blob is placed under a folder matching the category with the file name retained. For example, for an audio file named `MyAudio.ogg` matching `audio/ogg` MIME type having SHA256 digest `64988b348af3acf583d57260b44528f85da378b4dbcf4e55d36f9fac420a0ccd`:

```sh
~/.local/share/appName/
    ├── common/
    │   └── 64988b348af3acf583d57260b44528f85da378b4dbcf4e55d36f9fac420a0ccd
    └── audio/
        └── MyAudio.ogg -> ../common/64988b348af3acf583d57260b44528f85da378b4dbcf4e55d36f9fac420a0ccd
```

FileMan doesn't rely on any registry files for the store contents - it's *stateless*. It goes through various sanity and hardening checks for files being added and removed including, but not limited to path traversals, recursive symlink loops, DoS using big files, crafted symlinks pointing outside the store etc, and tries to follow least privilege principles. Unrecognized, stale, corrupted or unknown files are pruned during cleanup or during add/remove routines. It's highly recommended to use only the provided APIs to place and remove files in the store, placing files misappropiately can have unreliable consequences, e.g. being deleted or not shown.

FileMan relies heavily on [QMimeDatabase](https://doc.qt.io/qt-6/qmimedatabase.html), discards duplicate files during adding, opportunistacilly tries to hardlink the added file (UNIX only for now) and tries to lean towards performance. File copying is also fail safe - writes are committed atomically & failed transactions are discarded.

The code is intended to be C++17 standardized and Qt >=6.9 compatible.

> [!NOTE]
> `$XDG_DATA_HOME` defaults to `~/.local/share` if empty

***

## Documentation

<a name="category-enum"/>

#### *enum* FileMan::Category
#### *flag* FileMan::Categories

The enums describes the category options available on FileMan. These flags can be *OR*ed to specify multiple categories.

| Constant | Value | Description |
| :-- | :-- | :-- |
| `FileMan::Audio` | `0x01` | Operate on the **audio** store; located under `$XDG_DATA_HOME/appName/audio`. Takes only files with `audio/*` MIME types |
| `FileMan::Image` | `0x02` | Operate on the **image** store; located under `$XDG_DATA_HOME/appName/image`. Takes only files with `image/*` MIME types |
| `FileMan::Video` | `0x04` | Operate on the **video** store; located under `$XDG_DATA_HOME/appName/video`. Takes only files with `video/*` MIME types |
| `FileMan::All`   | `Audio` \| `Image` \| `Video` | Operate on all categories. Takes files with all preceding MIME types |

> [!Note]
> Qt's MIME database recognizes matroska (.mkv) files as application/x-matroska. These are whitelisted for `FileMan::Video`.

<a name="addfile"/>

#### [QVariantMap](https://doc.qt.io/qt-6/qvariant.html) FileMan::addFile([QUrl](https://doc.qt.io/qt-6/qurl.html) &fileURL, [FileMan::Categories](#fileman-category) category = FileMan::All)

Adds the file from *fileURL* to store with an optional *category* specified. The URL must follow local file specifiers: `file:///path/to/myfile.img` for absolute or `file:myfile.img` for relative path to `myfile.img`. Returns two KV-paired QVariantMap/struct output:

| Entry (Type) | Value (Type) |
|--|--|
| `error` (QString) | 0/`errno` (int) |
| `path` (QString) | `<path_to_symlink>`/empty (QString) |

Failed returns follow [`errno`](https://www.man7.org/linux/man-pages/man3/errno.3.html) values. On success (`"error": 0`), a path to the symlink under matching category is returned. For the example above, `"path": ~/.local/share/appName/audio/MyAudio.ogg`. Unsuccessful operations return corresponding non-zero `errno` value for `"error"` and an empty string for `"path"`.

ORing *category* adds *fileURL* to the first matching enum, even if it matches multiple categories. In this case, if adding fails, the error for adding the last category is returned. *category* can also be omitted, in which case, *fileURL* is matched against all valid categories. Due to performance impact, it is recommended to explicitly specify *category* and as narrowly as possible.

Duplicate detection is done on the blobs, not on symlink names. Adding duplicate files to the store, i.e. files having same digests, fails ambiguosly; path to the existing symlink is returned if it exists (an empty string otherwise) but unlike a successful operation, `EEXIST` is returned.

> [!Note]
> - Symlinks are added `.lnk` extension on Windows
>
> - Operations will continue even if storage info checks fail

> [!Warning]
> addFile() relies entirely on how QMimeDatabase recognizes the file. On some distributions, Qt's mime database is known to provide generic mime types even for obvious files in which case addFile() fails. This is an ongoing issue, if you encounter this, please do file a bug report.

Example:

```cpp
//  MIT License : Hasan Esa 2026

#include <QtCore>
#include <QTextStream>
#include <QVariant>
#include <HasanEsa/FileMan/fileman.hpp>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("myapp");

    FileMan fileman;
    QTextStream in(stdin);

    while (!in.atEnd()) {
        if (line.isEmpty()) continue;
        qInfo() << "Provide file URL:";
        QString fileUrl = in.readLine().trimmed();

        QVariantMap result = fileman.addFile(QUrl(fileUrl), FileMan::Audio | FileMan::Video);

        qInfo() << "Program returned saved path:" << result["path"].toString();
        if (int e = result["error"].toInt(); e != 0) return e;
    }
    return 0;
}
```

<a name="getdatadir"/>

#### [QString](https://doc.qt.io/qt-6/qstring.html) FileMan::getDataDir()

Returns a string to application data directory. This is `$XDG_DATA_HOME/<appName>/` or `~/.local/share/<appName>/` if `$XDG_DATA_HOME` is unset. Return string is never empty.

<a name="getfilename"/>

#### [QString](https://doc.qt.io/qt-6/qstring.html) FileMan::getFileName([QUrl](https://doc.qt.io/qt-6/qurl.html) &fileURL)

Translates the url *fileURL*, extracts & returns the filename component from it. If *fileURL* ends with a trailing `/`, the returned string is empty. Returned string is also empty if fails for other reasons.

```cpp
QString fileUrl = "file:///absolute/path to/.my/audio.ogg";
QString folderUrl = "file:///absolute/path to/.my/";

qInfo() << "FileName:" << fileman.getFileName(QUrl(fileUrl));
qInfo() << "FileName:" << fileman.getFileName(QUrl(folderPath));
// Output:
// FileName: "audio.ogg"
// FileName: ""
```

<a name="rmfile"/>

#### int FileMan::rmFile([QString](https://doc.qt.io/qt-6/qstring.html) fileName, [FileMan::Categories](#fileman-category) category = FileMan::All)

Removes the file with the name *fileName* from the application store under the optionally specified *category*. Paths are not necessary. Removes both the hashed blob and the symlink pointing to it. Returns 0 if successful and non-zero `errno` if *fileName* doesn't exist or failed to remove. Hashed blob is always removed first, so chances of blob orphanage is low.

rmFile() will remove *any* files or folders matching *fileName* from the categorized directory. Incomplete removing is possible, e.g. backing hashed file removed but symlink removal was unsuccessful. However, it is still possible to remove the dangling symlinks next time as a cause of this, provided the implementation handles returns properly.

ORing lets rmFile() look for additional category directories; files with the same name inside all supplied categories will also be removed. Returns successful if removal succeeds under any one of the category. If unsuccessful, returns the error for the last category where removal failed. Empty *category* lets rmFile() look for *fileName* in all valid categories available. Due to performance impact, it is recommended to explicitly specify *category*, and as narrowly as possible.

<a name="storehousekeeping"/>

#### int FileMan::storeHouseKeeping()

Executes cleanup routines to prune unknown, unrecognized & invalid files (e.g. dangling symlinks, corrupted blobs) from the store. Cleans from both "common" and categorized directories. Recommended to call on application startup or before exit. Returns 0 on success, `errno` on failure.

***

<a name="qml-integration">

## QML Integration Guide

FileMan can also be used as a QML module to integrate with QML apps very easily. With this approach, there's no need to set context properties from C++ backend into QML namespace which is also discouraged. There are two ways to integrate FileMan:

<a name="include-submodule"/>

### 1. Including as a submodule

This is a very common way of integrating third-party libraries within a project. Decide where to add the FileMan submodule first. For our guide here, we'll add it under `libs/` from the project root. Whichever location you choose, just make sure the trailing directories are `HasanEsa/FileMan/` complying with Qt's expected URI translated lookup:

```bash
git clone --depth 1 --branch v1.0.0 --recurse-submodules https://gitlab.com/hasanesa/fileman libs/HasanEsa/FileMan
git submodule add https://gitlab.com/hasanesa/fileman libs/HasanEsa/FileMan
```

On your project's main `CMakeLists` add/modify these invocations: [^1]

```CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

find_package(Qt6 REQUIRED COMPONENTS Qml)

add_subdirectory(libs/HasanEsa/FileMan)

qt_add_executable(${PROJECT_NAME} main.cpp)
qt_add_qml_module(${PROJECT_NAME}
    URI your.company.uri
    QML_FILES
        main.qml
        # Enlist *all* your .qml, .js & .mjs files here

    SOURCES
        main.cpp
        # Enlist *all* your .c/.cpp and .h/.hpp files here

    RESOURCES
        brandImg.png
        # Enlist *all* required resources

    DEPENDENCIES TARGET FileMan
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Qml FileMan)

# Required for clang+LLVM
target_link_options(${PROJECT_NAME} PRIVATE $<$<CXX_COMPILER_ID:Clang>:-fuse-ld=lld>)
```

Import the module on your qml file:

```qml
import HasanEsa.FileMan
```

You're now qualified to call and operate on all enums/flags, functions and members [FileMan](#fileman) provides:

```qml
var result = FileMan.addFile(fileUrl, FileMan.Audio | FileMan.Video)
```

> [!Important]
> FileMan's types and members are accessible only inside "FileMan" scope. Qt doesn't allow importing the module on a custom namespace.

---

For static building, build FileMan as [static](#available-build-flags) and link against the FileMan plugin instead:

```CMakelists.txt
set(FM_BUILD_AS "STATIC" CACHE STRING "Set FileMan build type")
# Alternatively, pass -DFM_BUILD_AS=STATIC during configuring cmake

target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Qml FileManplugin)
```

Add a QML module import macro on your `main.cpp`

```cpp
#include <QtQml/QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(HasanEsa_FileManPlugin)
```

Then import the module on your qml file as usual. Refer to the [QML example](examples/fileman-qml/) for a basic working implementation.


<a name="include-system-installed">

### 2. Using system-installed instance

> [!IMPORTANT]
> This requires FileMan to be [built & installed](#building) on your running system first.

On your project's main `CMakeLists` add/modify these invocations: [^1]

```CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

find_package(Qt6 REQUIRED COMPONENTS Qml)
find_package(FileMan REQUIRED)

qt_add_executable(${PROJECT_NAME} main.cpp)
qt_add_qml_module(${PROJECT_NAME}
    URI your.company.uri
    QML_FILES
        main.qml
        # Enlist *all* your .qml, .js & .mjs files here

    SOURCES
        main.cpp
        # Enlist *all* your .c/.cpp and .h/.hpp files here

    RESOURCES
        brandImg.png
        # Enlist *all* required resources

    DEPENDENCIES HasanEsa.FileMan
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Qml HasanEsa::FileMan)

# Required for clang+LLVM
target_link_options(${PROJECT_NAME} PRIVATE $<$<CXX_COMPILER_ID:Clang>:-fuse-ld=lld>)
```

Import and use the API:

```qml
import HasanEsa.FileMan

var result = FileMan.addFile(file://path/to/myVideo.mkv, FileMan.Audio | FileMan.Video)
```

---

For statically linking however, make sure the system-installed instance has been built [statically](#available-build-flags). On your root `CMakeLists` compared to previous, just modify these lines:

```CMakeLists.txt
target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Qml HasanEsa::FileManplugin)
```

Add a QML module import macro on your `main.cpp` as previously

```cpp
#include <QtQml/QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(HasanEsa_FileManPlugin)
```

Then import the module on your qml file as usual. Refer to the [QML example](examples/fileman-qml/) for a basic working implementation.

***

## Implementations

- [Munadi](https://gitlab.com/munadi/munadi)

***

## Contributing

This project uses CMake as the build environment. Minimum required is 3.16 with highest policy set to 4.3. clang with LLVM is the recommended compiler used for the project.

### Available Build Flags:

These flags control various behaviors of the build configuration. Prepend the following options/flags with `-D` when configuring cmake.

| Flag | Defaults | Available Values | Description |
| :-- | :--: | :--: | :-- |
| `FM_BUILD_AS` | `SHARED` | `STATIC`, `SHARED` | Sets the build type of the library. If not set, the default is to build shared libraries. |
| `FM_BUILD_EXAMPLES` | `OFF` | `ON`, `OFF` | Enable building example binaries in-tree along with the project. If not set, example binaries are skipped. |
| `BUILD_SHARED_LIBS` | undefined | `ON`, `OFF` | Compatibility alternative to `FM_BUILD_AS`. Controls library build type to be either shared (`ON`) or static (`OFF`). Undefined by default, which implies `FM_BUILD_AS=SHARED` |
| `CMAKE_INSTALL_PREFIX` | `/usr/local` | \<any valid path\> | Configures prefix directory to install under. Recommended to be one from `$PATH` variable. Some systems may default to `/usr`. For CMake >= 3.23, `cmake --prefix <path>` can also be used. |

Repository structure follows flat layout as recommended by [`qt_add_qml_module()`](https://doc.qt.io/qt-6/qt-add-qml-module.html#target-structure). In-source building is the preferred way, although out-of-source building is possible but issues a warning about QML module discovery. The latter can be approached for development builds.

### Building

The [repository registry](https://gitlab.com/hasanesa/fileman/container_registry) hosts builder images for development tasks tailored for FileMan.

First, from project root, create a container with the builder image and enter :
```bash
podman run --rm -it -v $(pwd):/src:z registry.gitlab.com/hasanesa/fileman/builder:latest
```
<details><summary>Further developer notes: Click to expand</summary>

---

To build the image yourself, from project root do:

```bash
podman build -t localhost/fileman-builder:latest images/
```

Then create the container and enter:
```bash
podman run --rm -it -v $(pwd):/src:z localhost/fileman-builder
```

---

</details>

Replace `podman` with `docker` if using docker instead.

From there execute:
```bash
CC=clang CXX=clang++ cmake -B . --fresh -GNinja && cmake --build . --parallel
```
Alternatively, just press up arrow key and hit enter! The built libraries can then be installed to the default prefix
```bash
# append --prefix path/to/prefix for custom location
cmake --install .
```

Separate component-wise installation is supported too. Two components are available: `runtime` & `devel`. To install for example, the development-related files only specify `--component devel` with the previous invocation:
```bash
cmake --install . --component devel
```

The repository contains example apps that can be opted in for building during configuration with `-DFM_BUILD_EXAMPLES=ON`. The built binaries would be available under their respective folders `examples/<program name>` and are not exported or installed. They can also be built standalone without the need for configuring the root project provided the library has already been installed to running system and available in searchable paths. This however, requires a manual change of include directive in `main.cpp` files from `#include <fileman.hpp>` to `#include <HasanEsa/FileMan/fileman.hpp>`. After ensuring the requirements, change working directory to the `examples/` folder, configure and build the binary:
```bash
cd examples/
CC=clang CXX=clang++ cmake -S . -B build/ --fresh -GNinja && cmake --build build/ --parallel
```

The built binary should be available under `build/`. Try executing the binary:
```bash
QT_LOGGING_RULES="*.debug=true;qt.*.debug=false;qt.qml.import=true" build/<app>
```
The `QT_LOGGING_RULES` environment variable helps to see detailed logs and useful debugging information. If executing outside the container where FileMan isn't installed, you may need to modify `LD_LIBRARY_PATH` & [`QML_IMPORT_PATH`](https://doc.qt.io/qt-6/qtqml-syntax-imports.html#qml-import-path) environment variables pointing to the appropriate directory holding the FileMan library and the QML module. If built statically however, this is not necessary.

***

If you want to set up the environment on your own or integrate with your own project, building FileMan requires `qt6-qtcore-devel`, `qt6-qtdeclarative-devel`, `ccache`, `cmake`, `clang`, `lld`, `llvm`, `nasm` & `ninja-build`.

> [!Note]
> Package names here are for Fedora Linux/EL distributions and their derivatives. Refer to your distribution for exact package names

For Qt6/C++ implementations, adjust your CMakeLists file to contain these command invocations in the least:

```CMakeLists
cmake_minimum_required(3.16)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(FileMan REQUIRED)

qt_standard_project_setup()

target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Core HasanEsa::FileMan)

# lld is required for clang, can be omitted if using gcc instead
target_link_options(${PROJECT_NAME} PRIVATE $<$<CXX_COMPILER_ID:Clang>:-fuse-ld=lld>)
```

## AI Contribution Policy

We try to be as clear as possible on AI agents, LLMs and bots contributions to the project. AI-assisted contributions by humans are welcome, any other autonomous AI contributions including merge/pull requests, issues & comments will be rejected. This includes any variants of "Signed-off by `<agent's operator>`" i.e. human-operated agentic requests. We encourage (human) contributors to include "Assisted by: " footer whenever presenting any contributions on the project if using any assistances by AIs & LLMs.

***

## License
This project is licensed under MIT. Redistribution permission, usage scope and copyrights apply as per the license.

***

[^1]: For full reference on `qt_add_qml_module()`: https://doc.qt.io/qt-6/qt-add-qml-module.html