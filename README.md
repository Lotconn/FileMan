# FileMan

Smart, Secure & Simple file manager library for Qt/QML apps written in Qt6/C++

---

FileMan provides simple APIs and a robust backend for managing the data store
for Qt/QML applications.

**Note:** FileMan is a singleton, moderately hardened, async-safe & thread-safe.

## FileMan Class

| Use case | Integration                                                                                                                                                                                                                                                         |
| -------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Header:  | `#include <Lotconn/FileMan/fileman>`                                                                                                                                                                                                                                |
| CMake:   | `find_package(FileMan REQUIRED)`<br>`target_link_libraries(mytarget PRIVATE Lotconn::FileMan)`<br>`target_link_libraries(mytarget PRIVATE Lotconn::FileManplugin)` \[static building\]<br>`qt_add_qml_module(mytarget DEPENDENCIES Lotconn.FileMan)` \[QML module\] |
| QML:     | `import Lotconn.FileMan`                                                                                                                                                                                                                                            |
| CPP:     | `#include <QtQml/QQmlExtensionPlugin>`<br>`Q_IMPORT_QML_PLUGIN(Lotconn_FileManPlugin)` \[static building\]                                                                                                                                                          |

### Public Types

| Type   | Description                  |
| ------ | ---------------------------- |
| struct | [add_file_t](#add-file-type) |
| enum   | [Category](#category-enum)   |
| flag   | [Categories](#category-enum) |
| enum   | [Errno](#errno-enum)         |
| struct | [rm_file_t](#rm-file-type)   |

### Public Functions

|                                                     Return type | Function                                                                                                                                      |
| --------------------------------------------------------------: | :-------------------------------------------------------------------------------------------------------------------------------------------- |
|                                                                 | [FileMan()](#fileman-constructor)                                                                                                             |
|                                                                 | [~FileMan()](#fileman-destructor)                                                                                                             |
|                                    [add_file_t](#add-file-type) | [addFile](#addfile)(_const_ [QUrl](https://doc.qt.io/qt-6/qurl.html) &fileUrl, [FileMan::Categories](#category-enum) category = FileMan::All) |
| [QString](https://doc.qt.io/qt-6/qstring.html)<br>[[nodiscard]] | [categoryToString](#categorytostring)([FileMan::Category](#category-enum) category) _const_                                                   |
| [QString](https://doc.qt.io/qt-6/qstring.html)<br>[[nodiscard]] | [getCategoryDir](#getcategorydir)([FileMan::Category](#category-enum) category) _const_                                                       |
| [QString](https://doc.qt.io/qt-6/qstring.html)<br>[[nodiscard]] | [getDataDir](#getdatadir)() _const_                                                                                                           |
|                  [QString](https://doc.qt.io/qt-6/qstring.html) | [getFileName](#getfilename)(_const_ [QUrl](https://doc.qt.io/qt-6/qurl.html) &fileUrl)                                                        |
|                                                             int | [refreshStore()](#refreshStore)                                                                                                               |
|                                      [rm_file_t](#rm-file-type) | [rmFile](#rmfile)([QUrl](https://doc.qt.io/qt-6/qurl.html) fileUrl)                                                                           |

### Static Public Members

|                                    Return type | Function                                                                             |
| ---------------------------------------------: | :----------------------------------------------------------------------------------- |
|                                        FileMan | &FileMan::[instance()](#fileman-instance)                                            |
| [QString](https://doc.qt.io/qt-6/qstring.html) | [getFileName](#getfilename)(const [QUrl](https://doc.qt.io/qt-6/qurl.html) &fileUrl) |

---

## Detailed Description

FileMan is a file manager backend for managing the application store. It creates
and manages the store under `<AppLocalDataLocation>` which on Unix expands as
`$XDG_DATA_HOME/<APPNAME>/` or `~/.local/share/<APPNAME>`, and
`C:/Users/<USER>/AppData/Local/<APPNAME>`, `<APPDIR>`, `<APPDIR>/data` or
`<APPDIR>/data/<APPNAME>` on Windows (`<APPDIR>` is the directory containing the
application executable). FileMan matches the MIME type of the file against the
specified [category](#category-enum) during adding, cryptographically hashes it
(currently using SHA256) and stores the raw "blob" with the digest name under
`<AppLocalDataLocation>/common`. A symlink to this blob is placed under a folder
matching the category with the file name retained. For example:

```cpp
//  MIT License : Lotconn 2026

#include <QtCore>
#include <fileman>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("myapp");
  FileMan fileman;

  const QUrl file("file:///home/user/Music/MyAudio.ogg");
  const add_file_t result = fileman.addFile(file, FileMan::Audio | FileMan::Video);

  if (result.status() != 0) {
    qWarning() << file.toDisplayString() << "adding failed";

    return 1;
  }

  qInfo() << "Saved location:" << result.location().toDisplayString();

  return 0;
}
```

outputs:

```sh
$ ./myapp
Saved location: file:///home/user/.local/share/myapp/audio/MyAudio.ogg
```

On a linux distribution, the audio file named `MyAudio.ogg` matching `audio/ogg`
MIME type having the SHA256 digest
`64988b348af3acf583d57260b44528f85da378b4dbcf4e55d36f9fac420a0ccd` will be
stored as:

```sh
~/.local/share/<APPNAME>/
    ├── common/
    │   └── 64988b348af3acf583d57260b44528f85da378b4dbcf4e55d36f9fac420a0ccd
    └── audio/
        └── MyAudio.ogg -> ../common/64988b348af3acf583d57260b44528f85da378b4dbcf4e55d36f9fac420a0ccd
```

### Performance, Security & Reliability

FileMan doesn't rely on any persistent file for registering its state - it's
_stateless_ by design. It instead uses a O(1) fast-lookup in-memory cache
representing the current store-state which helps bypassing majority of costly
IO. That too however, is validated at the cost of minimal O(1) IO checks with
on-disk store state before finally relying on. Despite so, memory requirements
for FileMan's internal processing alone is generally low and can be reliably
adopted on systems with constrained resources.

FileMan goes through various sanity and hardening checks for files being added
or removed including, but not limited to path traversals, recursive symlink
loops, DoS using big files, crafted symlinks pointing outside the store etc, and
tries to follow least privilege & RAII principles. Stale, corrupted, or
otherwise invalid files are pruned during add/remove operations or at explicit
store refresh requests. It's highly recommended to use only the provided APIs to
place and remove files in the store, placing files misappropriately can have
unforseeable consequences, e.g. being deleted or not registered.

FileMan relies heavily on
[QMimeDatabase](https://doc.qt.io/qt-6/qmimedatabase.html), reuses matching
duplicates or orphaned blobs during adding, opportunistically tries to hardlink
the added file and ensures performance while assuring thread-safety and
security. File copying is also fail safe - writes are committed atomically &
failed transactions are discarded.

The code is intended to be C++17 standardized and Qt >=6.8 compatible.

> [!Warning]
>
> FileMan will abort fatally if `<APPNAME>` resolves empty. Please ensure your
> implementation at least has `QCoreApplication::applicationName()` set and
> FileMan is instantiated only after that.

### The Singleton Design

Explicit instantiation is required as usual to access members other than static.
FileMan being a singleton means only one instance can ever exist per process,
and it's the first created instance. Attempt to create a second FileMan instance
will abort fatally. However, the already-created instance can be referred
externally without holding reference through `FileMan::instance()`:

```cpp
// A cleanup routine connected elsewhere, with no `fileman` in scope
void onAboutToQuit() {

  FileMan localMan; // <--- wrong! will abort fatally

  const int status = FileMan::instance().refreshStore(); // <-- right, call the
                                                        // already available
                                                       // instance

  if (status != 0)
    qWarning() << "refreshStore() failed on exit:" << status;
}

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("myapp");

  FileMan fileman; // the one and only instance, held locally here

  QObject::connect(&app, &QCoreApplication::aboutToQuit, &onAboutToQuit);

  return app.exec();
}
```

For QML integrations, FileMan exposes a QML_SINGELTON factory
`FileMan::*create()` which is invoked by QQmlEngine during accessing any public
members. In such cases, explicit instantiation from C++ backend is not
necessary. However, although discouraged, instantiation is necessary for
embedding FileMan's context properties in QML's namespace root. The recommended
method is to use FileMan as a QML module which is detailed in
[QML Integarion Guide](#qml-integration) section.

### Thread safety

FileMan doesn't use concurrency in-code, however, it is possible to async-call
FileMan's public APIs (e.g. from QML). In such cases, race conditions are
avoided by acquiring exclusive locks at the on-disk application store and on the
in-memory store-state cache during read & write asserts. Additional
thread-serialization is also ensured by implementing thread-locks/mutex during
async-critical processes. In broader view, FileMan methods _can_ be called
concurrently from any number-of-threads but with a wait-interrupt cost.

### Reentrancy

FileMan is not designed to be reentrant. APIs therefore cannot be recursively
called nor re-entered from the same thread before a previous call returns.

FileMan shouldn't be sub-classed. Although it's not defined to be a leaf class,
the QML singleton registration mechanism is Qt-internal and the class remains a
leaf type with respect to its public API. Consumers are therefore strongly
advised against it.

### Stability

FileMan is API-stable as of current version and is also expected to be
ABI-stable within a major version. ABI-stability however, is still not proven
yet, testers are called for. Proposed LTS release models are in consideration
and planned to be maintained for three-years at least.

### RESTful API conformity

Although FileMan doesn't work with network-bound/web URLs yet, the input/output
of the APIs closely follow RESTful API conventions (e.g. [addFile()](#addfile),
[rmFile()](#rmfile)). Network-bound/web URL support are in the plans.

### QML Support

It is also possible to integrate FileMan directly with QML as either a
dynamically loadable module or persistently linked static module. A C++ backend
is not required. For static linking the module, only a minimal C++ backend
addition is necessary whose sole purpose is to import the static module. For
details and elaborate guidance, refer to the
[QML Integration Guide](#qml-integration) section.

---

## Member Type Documentation

<div><a id="add-file-type"></a></div>
<div><a name="add-file-type"></a></div>

#### add_file_t

Struct type holding members _int_ type _status()_ and _QUrl_ type _location()_.
Both are marked `[[nodiscard]]`.

<div><a id="category-enum"></a></div>
<div><a name="category-enum"></a></div>

#### _enum_ FileMan::Category

#### _flag_ FileMan::Categories

These enums describe the category options available. They can be *OR*ed to
specify multiple categories.

| Constant         | Value                         | Description                                                                                                              |
| :--------------- | :---------------------------- | :----------------------------------------------------------------------------------------------------------------------- |
| `FileMan::None`  | `0x00`                        | Does nothing, for internal use only. Using it on public calls may crash programs                                         |
| `FileMan::Audio` | `0x01`                        | Operate on the **audio** store; located under `<AppLocalDataLocation>/audio`. Takes only files with `audio/*` MIME types |
| `FileMan::Image` | `0x02`                        | Operate on the **image** store; located under `<AppLocalDataLocation>/image`. Takes only files with `image/*` MIME types |
| `FileMan::Video` | `0x04`                        | Operate on the **video** store; located under `<AppLocalDataLocation>/video`. Takes only files with `video/*` MIME types |
| `FileMan::All`   | `Audio` \| `Image` \| `Video` | Operate on all categories. Takes files with all preceding MIME types                                                     |

> [!Note]
>
> Qt's MIME database recognizes matroska (.mkv) files as application/x-matroska.
> These are whitelisted for `FileMan::Video`.

<div><a id="errno-enum"></a></div>
<div><a name="errno-enum"></a></div>

#### _enum_ FileMan::Errno

These map the common API errno return enums to _FileMan_ scoped enums. Useful
for referring to return values from FileMan's APIs within QML or namespaces
having limited access to
[_cerrno_](https://en.cppreference.com/cpp/header/cerrno). Note, some non-POSIX
platforms may not support few errno values (e.g. Windows). Developers are
discouraged from using raw errno values as they may vary depending on platforms.

| Constant                    | Value              | Description                       |
| :-------------------------- | :----------------- | :-------------------------------- |
| `FileMan::Success`          | `0`                | Success value                     |
| `FileMan::Busy`             | `EBUSY`/`16`       | Device or resource busy           |
| `FileMan::Cancelled`        | `ECANCELLED`/`125` | Operation canceled                |
| `FileMan::Exists`           | `EEXIST`/`17`      | File exists                       |
| `FileMan::FileTooBig`       | `EFBIG`/`27`       | File too large                    |
| `FileMan::Invalid`          | `EINVAL`/`22`      | Invalid argument                  |
| `FileMan::IOError`          | `EIO`/`5`          | Input/output error                |
| `FileMan::Loop`             | `ELOOP`/`40`       | Too many levels of symbolic links |
| `FileMan::NotFound`         | `ENOENT`/`2`       | No such file or directory         |
| `FileMan::NotSupported`     | `EOPNOTSUPP`/`95`  | Operation not supported           |
| `FileMan::PermissionDenied` | `EPERM`/`1`        | Operation not permitted           |
| `FileMan::TryAgain`         | `EAGAIN`/`11`      | Resource temporary unavailable    |

<div><a id="rm-file-type"></a></div>
<div><a name="rm-file-type"></a></div>

#### rm_file_t

Struct type holding members _int_ type _status_l()_ and _int_ type _status_h()_.
Both are marked `[[nodiscard]]`.

## Member Function Documentation

<div><a id="fileman-constructor"></a></div>
<div><a name="fileman-constructor"></a></div>

#### FileMan()

Creates the _FileMan_ singleton instance.

<div><a id="fileman-destructor"></a></div>
<div><a name="fileman-destructor"></a></div>

#### ~FileMan()

Destroys the _FileMan_ singleton instance.

<div><a id="addfile"></a></div>
<div><a name="addfile"></a></div>

#### [add_file_t](#add-file-type) FileMan::addFile(_const_ [QUrl](https://doc.qt.io/qt-6/qurl.html) _&fileUrl_, [FileMan::Categories](#category-enum) _category_ = FileMan::All)

Adds the file from _fileUrl_ to the store with an optional _category_ specified.
The URL must follow local file specifiers: `file:///path/to/myfile.img` for
absolute or `file:myfile.img` for relative path to `myfile.img`. Returns a
[add_file_t](#add-file-type) struct:

| Member (Type)       | Value                                    |
| ------------------- | ---------------------------------------- |
| `status()` (int)    | 0/`errno`                                |
| `location()` (QUrl) | `<fully_qualified_url_to_symlink>`/empty |

Failed returns follow [`errno`](https://en.cppreference.com/cpp/header/cerrno)
values. On success (`status() == 0`), a path to the symlink in store is
returned. For the example above, `location()` holds
`file:///home/user/.local/share/<APPNAME>/audio/MyAudio.ogg`. Unsuccessful
operations return corresponding non-zero `errno` value for `status()` and an
empty URL for `location()`.

ORing _category_ adds _fileUrl_ to the first matching category flag, even if it
matches multiple categories. _category_ can also be omitted, in which case,
_fileUrl_ is matched against all valid categories. Due to this ambiguity, it is
recommended to explicitly specify _category_ as narrowly as possible.

Duplicate detection is done on the blobs, not on symlink names. Adding duplicate
files to the store, i.e. files having same digests, fails unambiguously; path to
the existing symlink is returned if it exists, or the orphaned blob is reused if
it is not linked to any other symlinks. However, unlike a successful operation,
`EEXIST` is returned.

> [!Note]
>
> - Symlinks bear `.lnk` extension on Windows
> - Operations will continue even if storage free-space checks fail

> [!Warning]
>
> addFile() relies entirely on how QMimeDatabase recognizes the file. On some
> distributions, Qt's mime database is known to provide generic mime types even
> for obvious files in which case addFile() fails. This is an ongoing issue, if
> you encounter this, please do file a bug report.

Example:

```cpp
//  MIT License : Lotconn 2026

#include <QtCore>
#include <QTextStream>
#include <Lotconn/FileMan/fileman>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("myapp");
    QTextStream in(stdin);
    FileMan fileman;

    while (!in.atEnd()) {

        qInfo() << "Provide file URL:";

        QString fileUrl = in.readLine().trimmed();

        if (fileUrl.isEmpty())
          continue;

        add_file_t result = fileman.addFile(QUrl(fileUrl));
        // omitting `category` means `fileUrl` is matched against all known
        // categories

        qInfo() << "Program returned saved path:" << result.location();

        if (int e = result.status(); e != 0)
          return e;
    }

    return 0;
}
```

<div><a id="categorytostring"></a></div>
<div><a name="categorytostring"></a></div>

#### _[[nodiscard]]_ [QString](https://doc.qt.io/qt-6/qstring.html) FileMan::categoryToString([FileMan::Category](#category-enum) category) _const_

Returns a [QString](https://doc.qt.io/qt-6/qstring.html) containing the name of
_category_. `FileMan::All` is a composite _flag_ and therefore not supported.

<div><a id="getcategorydir"></a></div>
<div><a name="getcategorydir"></a></div>

#### _[[nodiscard]]_ [QString](https://doc.qt.io/qt-6/qstring.html) FileMan::getCategoryDir([FileMan::Category](#category-enum) category) _const_

Returns a [QString](https://doc.qt.io/qt-6/qstring.html) containing the absolute
directory of _category_ items in store. `FileMan::All` is a composite _flag_ and
therefore not supported. Empty on invalid category.

<div><a id="getdatadir"></a></div>
<div><a name="getdatadir"></a></div>

#### _[[nodiscard]]_ [QString](https://doc.qt.io/qt-6/qstring.html) FileMan::getDataDir() _const_

Returns a [QString](https://doc.qt.io/qt-6/qstring.html) containing the absolute
directory of `<AppLocalDataLocation>`. String is never empty.

<div><a id="getfilename"></a></div>
<div><a name="getfilename"></a></div>

#### [QString](https://doc.qt.io/qt-6/qstring.html) FileMan::getFileName(_const_ [QUrl](https://doc.qt.io/qt-6/qurl.html) &fileURL)

Extracts and returns the filename component from _fileUrl_. Returns an empty
string if _fileURL_ ends with a trailing `/` or if resolution fails for any
other reason.

```cpp
QString fileUrl = "file:///absolute/path to/.my/audio.ogg";
QString folderUrl = "file:///absolute/path to/.my/";

qInfo() << "FileName:" << fileman.getFileName(QUrl(fileUrl));
qInfo() << "FileName:" << fileman.getFileName(QUrl(folderPath));
// Output:
// FileName: "audio.ogg"
// FileName: ""
```

<div><a id="instance"></a></div>
<div><a name="instance"></a></div>

#### FileMan &FileMan::instance()

<div><a id="refreshstore"></a></div>
<div><a name="refreshstore"></a></div>

Returns a reference to the already-constructed _FileMan_ singleton instance.
Meant for code that has no reference to the instance handed to it — e.g. a
helper function, a slot, or a different translation unit entirely — and would
otherwise need _FileMan_ passed down or made global just to reach it. Asserts if
called before any _FileMan_ instance has been constructed.

```cpp
// A cleanup routine connected elsewhere, with no `fileman` in scope
void onAboutToQuit() {

  FileMan localMan; // <--- wrong! will abort fatally

  const int status = FileMan::instance().refreshStore(); // <-- right, call the
                                                        // already available
                                                       // instance

  if (status != 0)
    qWarning() << "refreshStore() failed on exit:" << status;
}

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("myapp");

  FileMan fileman; // the one and only instance, held locally here

  QObject::connect(&app, &QCoreApplication::aboutToQuit, &onAboutToQuit);

  return app.exec();
}
```

#### int FileMan::refreshStore()

Executes cleanup routines on store pruning stale or invalid files (e.g. dangling
symlinks, corrupted or orphaned blobs), attempts generic fixes on store
directories (e.g. permission errors, non-existent or symlinked directories) and
rebuilds store-state cache. Highly recommended to be exposed as an interactable
state like [`addFile()`](#addfile) or [`rmFile()`](#rmfile). Implementations
should at least call the method on process startup or before exit. Returns 0 on
success, `errno` on failure.

<div><a id="rmfile"></a></div>
<div><a name="rmfile"></a></div>

#### [rm_file_t](#rm-file-type) FileMan::rmFile(_const_ [QUrl](https://doc.qt.io/qt-6/qurl.html) &fileUrl)

Removes the file _fileUrl_ from the application store. _fileUrl_ must be a
fully-qualified local URL referring to a file in store. Removes both the
referred file and the backing hashed blob. Returns a [rm_file_t](#rm-file-type)
struct:

| Member (Type)      | Values       |
| ------------------ | ------------ |
| `status_l()` (int) | 0/`errno`    |
| `status_h()` (int) | 0/-1/`errno` |

_fileUrl_ validations are always performed before its backing blob. Failed
returns follow [`errno`](https://en.cppreference.com/cpp/header/cerrno) values.
`status_l()` is non-zero if _fileUrl_ doesn't exist, is not a symlink, points to
an invalid location or when failed to remove. `status_h()` is `-1` if `rmFile()`
fails early before backing blob checks and non-zero positive if the backing
hashed blob doesn't exist or failed to remove.

rmFile() follows RESTful API convention; it will remove valid symlink files that
_fileUrl_ refers to within the categorized directories provided it exists but
will refuse if _fileUrl_ points to any other location outside the categorized
store. Incomplete removal is possible, e.g. backing hashed file removed but
symlink removal was unsuccessful. However, the resulting dangling symlinks can
be cleaned up on the next call or using [refreshStore()](#refreshstore),
provided the caller checks error returns appropriately. The backing hashed blob
is always removed first, so chances of blob orphanage is low.

---

<div><a id="qml-integration"></a></div>
<div><a name="qml-integration"></a></div>

## QML Integration Guide

FileMan can also be used as a QML module to integrate with QML apps very easily.
With this approach, there's no need to set context properties from C++ backend
into QML namespace which is also discouraged. There are three conventional ways
to integrate FileMan:

<div><a id="include-fetchcontent"></a></div>
<div><a name="include-fetchcontent"></a></div>

### 1. Using CMake `FetchContent` module

If you're using CMake as your build system, `FetchContent` module makes it very
easy to include FileMan as a third-party lib.

On your project's main `CMakeLists` add/modify these invocations: [^1]

```CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
include(FetchContent)

FetchContent_Declare(FileMan
  GIT_REPOSITORY  https://gitlab.com/lotconn/fileman
  GIT_TAG         v2.0.0 # Or use GIT_COMMIT <full commit shasum>
)
FetchContent_MakeAvailable(FileMan)

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

target_link_libraries(${PROJECT_NAME} PRIVATE FileMan)
```

---

For static building, include the directives to build FileMan as
[static](CONTRIBUTING.md/#available-build-flags) and link against the FileMan
plugin instead:

```CMakelists.txt
find_package(Qt6 REQUIRED COMPONENTS Qml)

# ...
set(FM_BUILD_AS "STATIC" CACHE STRING "Set FileMan build type")
FetchContent_MakeAvailable(FileMan)
# Alternatively, pass -DFM_BUILD_AS=STATIC during configuring cmake

target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Qml FileManplugin)
```

Add a QML module import macro on your `main.cpp`. For QML-only implementations,
this is the only inclusion necessary:

```cpp
#include <QtQml/QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(Lotconn_FileManPlugin)
```

<div><a id="include-submodule"></a></div>
<div><a name="include-submodule"></a></div>

### 2. Including as a submodule

This is a very common way of integrating third-party libraries within a project.
Decide where to add the FileMan submodule first. For our guide here, we'll add
it under `libs/` from the project root. Whichever location you choose, just make
sure the trailing directories are `Lotconn/FileMan/` complying with Qt's
expected URI translated lookup:

```bash
git clone --depth 1 --branch v2.0.0 --recurse-submodules https://gitlab.com/lotconn/fileman libs/Lotconn/FileMan
git submodule add https://gitlab.com/lotconn/fileman libs/Lotconn/FileMan
```

On your project's main `CMakeLists` just replace the `FetchContent` block with
this: [^1]

```CMakeLists.txt
# everything before FetchContent_Decalre()

add_subdirectory(libs/Lotconn/FileMan)

# everything after FetchContent_MakeAvailable()

```

The rest follows the same suit as previous.

<div><a id="include-system-installed"></a></div>
<div><a name="include-system-installed"></a></div>

### 3. Using system-installed instance

> [!IMPORTANT]
>
> This requires FileMan to be [built & installed](CONTRIBUTING.md/#building) on
> your running system first.

Following previous examples just modify these lines:

```CMakeLists.txt
qt_add_qml_module(${PROJECT_NAME}
    # ...

    DEPENDENCIES Lotconn.FileMan
)

# ...

target_link_libraries(${PROJECT_NAME} PRIVATE Lotconn::FileMan)
```

For statically linking however, make sure the system-installed instance has been
built [statically](CONTRIBUTING.md/#available-build-flags). Compared to previous
examples, just modify these lines:

```CMakeLists.txt
find_package(Qt6 REQUIRED COMPONENTS Qml)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Qml Lotconn::FileManplugin)
```

---

After completing either of the above methods, import the module on your qml
file:

```qml
import Lotconn.FileMan
```

You're now qualified to call and operate on all enums/flags, functions and
members [FileMan](#fileman) provides:

```qml
var result = FileMan.addFile(fileUrl, FileMan.Image)

console.log("Saved path:", result.location())
```

> [!Important]
>
> FileMan's types and members are accessible only inside "FileMan" scope. Qt
> doesn't allow importing the module on a custom namespace.

Refer to the [QML example](examples/fileman-qml/) for a basic working
implementation.

> [!Important]
>
> Example QML apps in the project built statically inside the container are not
> portable, they fail to run due to missing QT_PRIVATE_API, and won't run
> properly outside the container itself. Build these static apps on your host
> instead if you want to run them. Alternatively, build them on a statically
> built Qt system for them to be truly portable.

---

## Implementations

- ### Munadi
  - Homepage: <https://munadi.org>
  - VCS Browser: <https://gitlab.com/munadi/munadi>

---

## Contributing

This project uses CMake as the build environment. Minimum required is 3.16 with
highest policy set to 4.4. clang with LLVM is the recommended compiler used for
the project. See [CONTRIBUTING.md](./CONTRIBUTING.md) for further details about
building, available build flags and contribution guidelines & policies.

---

## License

This project is licensed under MIT, copyright Lotconn, 2026.

---

## Acknowledgements

### Libraries used in this project

- **Qt** by The Qt Company
  - Project Implementation: UI and core code
  - License: GPL v3 and LGPL v3
  - Homepage: <https://www.qt.io/>
  - Source: <https://code.qt.io/cgit/>

---

### Contributors

- **Project Author:** Hasan Esa [@HasanEsa](https://gitlab.com/HasanEsa)

---

[^1]:
    For full reference on `qt_add_qml_module()`:
    <https://doc.qt.io/qt-6/qt-add-qml-module.html>
