# FileMan

Simple Qt6 file manager class in C++

---

FileMan provides simple APIs and robust backend for managing data store for Qt applications.

**Note:** FileMan is moderately hardened, async-safe, thread-safe and is reentrant.

### Public Types
| Type | Description |
|------|-------------|
| enum | [Category](#category-enum) |

### Public Functions

| Return type | Function |
|--:|:--|
| [QVariantMap](https://doc.qt.io/qt-6/qvariant.html) | [addFile](#addfile)([QUrl](https://doc.qt.io/qt-6/qurl.html) &fileURL, [FileMan::Category](#fileman-category) category) |
| [QString](https://doc.qt.io/qt-6/qstring.html) | [getFileName](#getfilename)([QUrl](https://doc.qt.io/qt-6/qurl.html) &fileURL) |
| int | [rmFile](#rmfile)([QString](https://doc.qt.io/qt-6/qstring.html) fileName, [FileMan::Category](#fileman-category) category) |
| int | [storeHouseKeeping()](#storehousekeeping) |

### Static Public Members

| Return type | Function |
|--:|:--|
| [QString](https://doc.qt.io/qt-6/qstring.html) | getDataDir() |

***

## Detailed Description
FileMan is a file manager backend for managing application store. It creates and manages application store under `$XDG_DATA_HOME/appName/`. FileMan matches the MIME type of the file against the [category](#category-enum) during adding, cryptographically hashes it (currently using SHA256) and stores the raw "blob" with the digest name under `$XDG_DATA_HOME/appName/common`. A symlink to this blob is placed under categorized folder matching the category with the file name retained. For example, for an audio file named `MyAudio.ogg` matching `audio/ogg` MIME type, and computed SHA256 digest `64988b348af3acf583d57260b44528f85da378b4dbcf4e55d36f9fac420a0ccd`:

```sh
~/.local/share/appName/
    ├── common/
    │   └── 64988b348af3acf583d57260b44528f85da378b4dbcf4e55d36f9fac420a0ccd
    └── audio/
        └── MyAudio.ogg -> ../common/64988b348af3acf583d57260b44528f85da378b4dbcf4e55d36f9fac420a0ccd
```

FileMan doesn't rely on any registry file for store contents - it's *stateless*. It goes through various sanity and hardening checks for files being added and removed including, but not limited to path traversals, symlink loops, file bombs, crafted symlinks pointing outside the store etc and tries to follow least privilege principles. Unrecognized, stale, corrupted or unknown files are pruned during cleanup or during add/remove routines. It's highly recommended to use only the provided APIs to place and remove files in the store, placing files misappropiately can have unreliable consequences, e.g. being deleted or not shown.

FileMan relies heavily on [QMimeTypeDatabase](https://doc.qt.io/qt-6/qmimedatabase.html), discards duplicate files during adding, opportunistacilly tries to hardlink (UNIX only for now) and tries to lean towards performance. File copying is also fail safe - writes are committed atomically & failed transactions are discarded.

The code is intended to be C++17 standardized and Qt >=6.9 compatible.  

> [!NOTE]
> `$XDG_DATA_HOME` defaults to `~/.local/share` if empty

***

## Documentation

<a name="category-enum"/>

#### FileMan::Category

This enum describes the available category options available on FileMan. These enums are meant to be mutually exclusive. *OR*ing is not supported.

| Constant | Value | Description |
| :-- | :-- | :-- |
| `FileMan::Audio` | `0` | Add to or remove from the **audio** store; located under `$XDG_DATA_HOME/appName/audio`. Takes only files with `audio/*` MIME types |
| `FileMan::Image` | `1` | Add to or remove from the **image** store; located under `$XDG_DATA_HOME/appName/image`. Takes only files with `image/*` MIME types |

#### [QVariantMap](https://doc.qt.io/qt-6/qvariant.html) FileMan::addFile([QUrl](https://doc.qt.io/qt-6/qurl.html) &fileURL, [FileMan::Category](#fileman-category) category)
<a name="addfile"/>

Adds the file from *fileURL* to store. The URL must follow local file specifiers: `file:///path/to/myfile.img` for absolute or `file:myfile.img` for relative path to `myfile.img`. Returns two KV-paired QVariantMap output:

| Entry (Type) | Value (Type) |
|--|--|
| `error` (QString) | 0/`errno` (int) |
| `path` (QString) | `<path_to_symlink>`/empty (QString) |

Failed returns follow [`errno`](https://www.man7.org/linux/man-pages/man3/errno.3.html) values. On success (`"error": 0`), a path to the symlink under matching category is returned. For the example above, `"path": ~/.local/share/appName/audio/MyAudio.ogg`. Unsuccessful operations return corresponding non-zero `errno` value for `"error"` and an empty string for `"path"`.

Duplicate detection is done on the blobs, not on symlink names. Adding duplicate files to the store, i.e. files having same digests, fails ambiguosly; path to the existing symlink is returned if it exists (an empty string otherwise) but unlike a successful operation, `EEXIST` is returned.

> [!Note]
> - Symlinks are added `.lnk` extension on Windows
>
> - Storage info checks may fail silently

Example:

```cpp
#include <QtCore>
#include <QTextStream>
#include <QVariant>
#include "fileman.h"

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

        QVariantMap result = fileman.addFile(QUrl(fileUrl), category);

        qInfo() << "Program returned saved path:" << result["path"].toString();
        if (int e = result["error"].toInt(); e != 0) return e;
    }
    return 0;
}
```

#### [QString](https://doc.qt.io/qt-6/qstring.html) FileMan::getDataDir()
<a name="getdatadir"/>

Outputs the application data directory. This is `$XDG_DATA_HOME/appName/` or `~/.local/share/appName` if `$XDG_DATA_HOME` is unset. The return string is never empty.

#### [QString](https://doc.qt.io/qt-6/qstring.html) FileMan::getFileName([QUrl](https://doc.qt.io/qt-6/qurl.html) &fileURL)
<a name="getfilename"/>

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

#### int FileMan::rmFile([QString](https://doc.qt.io/qt-6/qstring.html) fileName, [FileMan::Category](#fileman-category) category)
<a name="rmfile"/>

Removes the file with the name *fileName* from store. Paths are not necessary. Removes both the hashed blob and the symlink pointing to it. Returns 0 if successful and non-zero `errno` if *fileName* doesn't exist or failed to remove. Hashed blob is always removed first, so chances of blob orphanage is low. 

rmFile() will remove *any* files or folders matching *fileName* from the categorized directory. Incomplete removing is possible, e.g. hashed file removed but symlink removal was unsuccessful. However, it is still possible to remove the dangling symlinks next time as a cause of this, provided the implementation handles returns properly.

#### int FileMan::storeHouseKeeping()
<a name="storehousekeeping"/>

Executes cleanup routines to prune unknown, unrecognized & invalid files (e.g. dangling symlinks, corrupted blobs) from the store. Cleans from both "common" and categorized directories. Recommended to call on application startup or before exit. Returns 0 on success, `errno` on failure. 

***

## Implementations

- [Munadi](https://gitlab.com/munadi/munadi)

***

## Contributing

The repository contains a cmdline example app with app name `myapp` and a `Containerfile` blueprint to test out FileMan right away. Clone the repository & `cd` to the root of the project:

```bash
git clone https://gitlab.com/hasanesa/fileman && cd fileman/
```

Then do:

```bash
podman -t localhost/test-builder .
```

Create and enter the container with this image:
```bash
podman run --rm -it -v $(pwd):/src localhost/test-builder
```

Replace `podman` with `docker` if using docker instead. From there execute:
```bash
cmake -S . -B build --fresh -G'Ninja Multi-Config' -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release --parallel && strip build/Release/*
```

Alternatively, just press up arrow key and hit enter! The built binary should be available under `build/Release/fileman`. Execute from outside the container:
```bash
QT_LOGGING_RULES="*.debug=true;qt.*.debug=false" build/Release/fileman
```
to see all logs & outputs.


If you want to set up the environment on your own or integrate on your own project, building FileMan requires `qt6-qtcore-devel`, `ccache`, `cmake`, `diffutils`, `gcc-c++`, `nasm` & `ninja-build`.

> [!Note]
> Package names here are for Fedora Linux/EL distros and derivatives. Refer to your distro for exact names

Adjust your `CMakeLists.txt` to contain these command invocations in the least:

```CMakeLists.txt
find_package(Qt6 REQUIRED COMPONENTS Core)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

qt_add_executable(#[[<binary name>]] main.cpp fileman.cpp fileman.h #[[...]])
target_compile_definitions(#[[<binary name>]] PRIVATE QT_ENABLE_STRICT_MODE_UP_TO=0x060900)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
```

## AI Contribution Policy

We try to be as clear as possible on AI agents, LLMs and bots contributions to the project. AI-assisted contributions by humans are welcome, any other autonomous AI contributions including merge/pull requests, issues & comments will be rejected. This includes any variants of "Signed-off by `<agent's operator>`" i.e. human-operated agentic requests. We encourage (human) contributors to include "Assisted by: " footer whenever presenting any contributions on the project if using any assistances by AIs & LLMs.

***

## License
This project is licensed under MIT. Redistribution permission, usage scope and copyrights apply as per the license.

***
