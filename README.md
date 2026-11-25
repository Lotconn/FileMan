# FileMan

Smart, Secure & Simple file manager library for Qt/QML apps written in Qt6/C++

---

FileMan provides simple APIs and a robust backend for managing the data store for Qt/QML
applications.

**Note:** FileMan is a singleton, moderately hardened, async-safe & thread-safe.

## FileMan Class

| Component      | Context                 | Code / Configuration                                                                   |
| :------------- | :---------------------- | :------------------------------------------------------------------------------------- |
| **Header**     | C++                     | `#include <fileman>` / `#include <Lotconn/FileMan/fileman>`                            |
| **CMake**      | Setup                   | `find_package(FileMan REQUIRED)`                                                       |
|                | Linking                 | `target_link_libraries(mytarget PRIVATE Lotconn::FileMan)`                             |
|                | Static Build (QML only) | `target_link_libraries(mytarget PRIVATE Lotconn::FileManplugin)`                       |
| **QML Module** | QML Module              | `qt_add_qml_module(mytarget DEPENDENCIES Lotconn.FileMan)`                             |
|                | QML                     | `import Lotconn.FileMan`                                                               |
|                | C++ Plugin              | `#include <QtQml/QQmlExtensionPlugin>`<br>`Q_IMPORT_QML_PLUGIN(Lotconn_FileManPlugin)` |

### Public Types

| Type   | Description                  |
| ------ | ---------------------------- |
| struct | [add_file_t](#add-file-type) |
| enum   | [Category](#category-enum)   |
| flag   | [Categories](#category-enum) |
| enum   | [Error](#error-enum)         |
| struct | [rm_file_t](#rm-file-type)   |

### Public Methods

|                           Return type | Function                                                                                                                                                     |
| ------------------------------------: | :----------------------------------------------------------------------------------------------------------------------------------------------------------- |
|                                       | [FileMan](#fileman-constructor-1)([QObject][qobject] \*parent = nullptr) `[explicit]`                                                                        |
|                                       | [FileMan](#fileman-constructor-2)(const [QDir][qdir] &storeDir, [QObject][qobject] \*parent = nullptr) `[explicit]` `[since 3.0]`                            |
|                                       | [FileMan](#fileman-constructor-3)(const [QString][qstring] &appName, [QObject][qobject] \*parent = nullptr) `[explicit]` `[since 3.0]`                       |
|                                       | [FileMan](#fileman-constructor-4)(const [QString][qstring] &orgName, const [QString][qstring] &appName, [QObject][qobject] \*parent = nullptr) `[since 3.0]` |
|                                       | [~FileMan()](#fileman-destructor) `[override]`                                                                                                               |
|          [add_file_t](#add-file-type) | [addFile](#addfile-1)(_const_ [QUrl][qurl] &fileUrl, bool overwrite, [FileMan::Categories](#category-enum) categories = FileMan::All) `[since 3.0]`          |
|          [add_file_t](#add-file-type) | [addFile](#addfile-2)(_const_ [QUrl][qurl] &fileUrl, [FileMan::Categories](#category-enum) categories = FileMan::All)                                        |
| [QString][qstring]<br>`[[nodiscard]]` | [categoryDir](#categorydir)([FileMan::Category](#category-enum) category) _const_ `[since 3.0]`                                                              |
|                                   int | [refreshStore()](#refreshstore)                                                                                                                              |
|            [rm_file_t](#rm-file-type) | [rmFile](#rmfile-1)([QUrl][qurl] fileUrl)                                                                                                                    |
|            [rm_file_t](#rm-file-type) | [rmFile](#rmfile-2)([QString][qstring] fileId)                                                                                                               |
| [QString][qstring]<br>`[[nodiscard]]` | [storeDir](#storedir)() _const_ `[since 3.0]`                                                                                                                |

### Static Public Members

|        Return type | Function                                                                                               |
| -----------------: | :----------------------------------------------------------------------------------------------------- |
| [QString][qstring] | [fileName](#filename)(const [QUrl][qurl] &fileUrl) `[since 3.0]`                                       |
|  FileMan::Category | [fromStringToCategory](#from-string-to-category)(const [QString][qstring] &categoryName) `[since 3.0]` |
|     FileMan::Error | [fromStringToError](#from-string-to-error)(const [QString][qstring] &errorName) `[since 3.0]`          |
|            FileMan | &FileMan::[instance()](#instance)                                                                      |
|               bool | &FileMan::[instanceExists()](#instance-exists) `[since 3.0]`                                           |
| [QString][qstring] | [toString](#tostring-1)([FileMan::Category](#category-enum) category) `[since 3.0]`                    |
| [QString][qstring] | [toString](#tostring-2)([FileMan::Error](#error-enum) error) `[since 3.0]`                             |

## Detailed Description

FileMan is a file manager backend for managing the application store. It creates and manages a
_content-addressable_ store under `<AppLocalDataLocation>/store` matching the MIME type of the file
against the specified [categories](#category-enum). `<AppLocalDataLocation>` on Unix expands as
`$XDG_DATA_HOME/<APPNAME>/` or `~/.local/share/<APPNAME>`, and
`C:/Users/<USER>/AppData/Local/<APPNAME>`, `<APPDIR>`, `<APPDIR>/data` or `<APPDIR>/data/<APPNAME>`
on Windows (`<APPDIR>` is the directory containing the application executable). During adding, the
file is cryptographically hashed (currently using SHA256) to uniquely identify it. The raw file data
(the "blob") is stored in a central `<AppLocalDataLocation>/store/common` folder, named exactly
after this hash digest. Then, a relative _symbolic link_ (symlink) is created in the specific
category folder (e.g., `audios/`), retaining the original, human-readable filename but pointing to
the hashed blob. For example:

```cpp
//  MIT License : Lotconn 2026

#include <QtCoreApplication>
#include <FileMan>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("myapp");
  FileMan fileman;

  const auto result = fileman.addFile(QUrl{ "file:///home/user/Music/MyAudio.ogg" });

  qInfo() << "Saved location:" << result.location().toDisplayString();

  return 0;
}
```

This example shows an [_addFile()_](#addfile-1) usage. On a Linux system, assuming the file matches
the `audio/ogg` MIME type and yields the shown SHA256 digest, the on-disk structure will look like
this:

```sh
~/.local/share/myapp/store
  |-- common/
  |   `-- 64988b348af3acf583d57260b44528f85da378b4dbcf4e55d36f9fac420a0ccd
  `-- audios/
      `-- MyAudio.ogg -> ../common/64988b348af3acf583d57260b44528f85da378b4dbcf4e55d36f9fac420a0ccd
```

The program output will be:

```sh
$ ./myapp
Saved location: file:///home/user/.local/share/myapp/store/audios/MyAudio.ogg
```

This returned URL is what the implementing application uses to process the file, e.g. removing it at
a later time using [_rmFile()_](#rmfile-1). Keen observers will notice that the last two components
of this URL directly identify the file's category and name within the store, resembling the _RESTful
member-type_ convention. Refer to [Member Type Documentation](#member-type-documentation) &
[Member Function Documentation](#member-function-documentation) for further details.

### Security-by-design

Before attempting any file operations, FileMan runs various integrity validations on the store
itself - e.g. checking for missing expected folders, mismatched folder types, or permission errors -
and attempts automatic remediation where safe. If the error is beyond FileMan's scope to fix
however, it aborts immediately.

When processing a new file, FileMan enforces strict sanity and hardening checks to protect the host
application. These include, but are not limited to:

- Preventing _path traversal_ attacks (e.g., `../../../etc/passwd`).
- Detecting and blocking recursive _symlink loops_.
- Mitigating Denial-of-Service (DoS) attempts via massive "file-bombs" by enforcing size limits.
- Rejecting crafted symlinks that point outside the designated store.

Throughout this process, FileMan adheres to the principle of least privilege and _RAII_ (Resource
Acquisition Is Initialization) to ensure resources are cleanly managed. Any stale, corrupted, or
invalid files encountered during the operation are safely pruned. Consequently, if a user manually
places or removes files in the store outside of the provided APIs, FileMan may delete or ignore
them. This is not a bug; it is the intended design to maintain absolute store integrity.

> [!Important]
>
> While FileMan establishes rudimentary hardening measures, it does not _guarantee_ absolute
> security; implementers must still ensure their own downstream security practices.

### Performance

FileMan doesn't rely on any persistent file for registering its state - it's _stateless_ by design.
It instead relies on a fast, O(1) in-memory cache (a dictionary) representing the current store
state. This cache bypasses the majority of expensive disk I/O operations, such as reverse lookups of
a hashed blob for existing symlinks. To ensure absolute reliability, this in-memory cache is
validated against the on-disk store state with minimal O(1) I/O checks before being fully trusted.
Despite this caching layer, FileMan's memory footprint remains exceptionally low, making it reliable
even on resource-constrained embedded systems.

### Efficiency & Reliability

FileMan adopts opportunistic efficiency on both its internal and public API routines. If such
attempts fail, fall-backs are implemented to ensure operations are still reliable and seemless. For
further details on the design, see [_addFile()_](#addfile-1), [_refreshStore()_](#refreshstore) &
[_rmFile()_](#rmfile-1).

The code is intended to be C++20 standardized and Qt >= 6.12 compatible.

> [!Warning]
>
> FileMan may abort fatally if `<APPNAME>` resolves empty. Please ensure your implementation at
> least has `QCoreApplication::applicationName()` set and instantiate FileMan only after that.

### The Singleton Design

Explicit instantiation is required as usual to access members other than static. FileMan being a
singleton means only one instance can ever exist per process, and it's the first created instance.
Attempt to create a second FileMan instance will abort fatally. However, the already-created
instance can be referred externally without holding reference through `FileMan::instance()`:

```cpp
// A cleanup routine connected elsewhere, with no `fileman` in scope
void onAboutToQuit() {

  // FileMan localMan; <-- WRONG! will abort fatally

  const int status = FileMan::instance().refreshStore(); // <-- CORRECT, call the already available
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

For QML integrations, explicit instantiation from C++ backend is not necessary. However, although
discouraged, instantiation is necessary for embedding FileMan's context properties in QML's
namespace root. The recommended method is to use FileMan as a QML module which is detailed in
[QML Integarion Guide](#qml-integration) section.

### Thread safety

FileMan doesn't use concurrency in-code, however, it is possible to async-call FileMan's public
APIs. To guarantee thread safety, FileMan acquires exclusive read/write locks on both the on-disk
store and the in-memory cache dictionaries before performing sensitive operations. These locks are
released at the earliest possible moment to prevent bottlenecks and mitigate deadlocks or race
conditions.

### Reentrancy

FileMan is not designed to be reentrant. APIs therefore cannot be recursively called nor re-entered
from the same thread before a previous call returns.

FileMan shouldn't be sub-classed. Although it's not defined to be a leaf class, the QML singleton
registration mechanism is Qt-internal and the class remains a leaf type with respect to its public
API. Consumers are therefore strongly advised against it.

### Stability

FileMan is API-stable as of current version and is also expected to be ABI-stable within a major
version. ABI-stability however, is still not proven yet, testers are called for. Proposed LTS
release models are in consideration and planned to be maintained for three-years at least.

### RESTful API resemblance

Although FileMan doesn't work with network-bound/web URLs yet, the input/output of the APIs closely
follow REST member ADD/DELETE API conventions (e.g. [addFile()](#addfile-1), [rmFile()](#rmfile-1)).
Network-bound/web URL support are in the plans. If it's rather preferred otherwise, FileMan's APIs
can still be operated with classic syntax model using IDs.

### QML Support

It is also possible to integrate FileMan directly with QML as either a dynamically loadable module
or persistently linked static module. A C++ backend is not required in this case. For static linking
the module, only a minimal C++ backend addition is necessary whose sole purpose is to import the
static module. For details and elaborate guidance, refer to the
[QML Integration Guide](#qml-integration) section.

## Platform Support

FileMan supports every platform and toolchain that
[Qt supports](https://doc.qt.io/qt-6.12/supported-platforms.html), owing to extensive use of
Qt-idiomatic codes. The library built on any new linux host for any target should work without
issues. For building-host platform support see
[the Supported Platforms section](CONTRIBUTING#supported-platforms) on CONTRIBUTING.md.

## Member Type Documentation

<div><a id="add-file-type"></a></div>
<div><a name="add-file-type"></a></div>

### add_file_t

Type class holding properties _status_, _location_ and _id_:

| Property | Type                           |
| -------- | ------------------------------ |
| status   | int                            |
| location | [url][url]                     |
| id       | [string][string] `[since 3.0]` |

#### Access Functions

| Type                                | Method                                          |
| ----------------------------------- | ----------------------------------------------- |
| int<br>`[nodiscard]`                | status()                                        |
| void                                | setStatus(int status) `[since 3.0]`             |
| [QUrl][qurl]<br>`[nodiscard]`       | location()                                      |
| void                                | setLocation(const QUrl &location) `[since 3.0]` |
| [QString][qstring]<br>`[nodiscard]` | id() `[since 3.0]`                              |
| void                                | setId(const QString &id) `[since 3.0]`          |

<div><a id="category-enum"></a></div>
<div><a name="category-enum"></a></div>

### _enum_ FileMan::Category

### _flag_ FileMan::Categories

These enums describe the category options available. They can be *OR*ed to specify multiple
categories.

| Constant         | Value                         | Description                                                                                                                     |
| :--------------- | :---------------------------- | :------------------------------------------------------------------------------------------------------------------------------ |
| `FileMan::None`  | `0x00`                        | Does nothing, for internal use only. Using it on public calls may crash programs                                                |
| `FileMan::Audio` | `0x01`                        | Operate on the **audio** store; located under `<AppLocalDataLocation>/store/audios`. Takes only files with `audio/*` MIME types |
| `FileMan::Image` | `0x02`                        | Operate on the **image** store; located under `<AppLocalDataLocation>/store/images`. Takes only files with `image/*` MIME types |
| `FileMan::Video` | `0x04`                        | Operate on the **video** store; located under `<AppLocalDataLocation>/store/videos`. Takes only files with `video/*` MIME types |
| `FileMan::All`   | `Audio` \| `Image` \| `Video` | Operate on all categories. Takes files with all preceding MIME types                                                            |

> [!Note]
>
> Qt's MIME database recognizes matroska (.mkv) files as application/x-matroska. These are
> whitelisted for `FileMan::Video`.

<div><a id="error-enum"></a></div>
<div><a name="error-enum"></a></div>

### _enum_ FileMan::Error

These map the common API errno return enums to _FileMan_ scoped enums. Useful for referring to
return values from FileMan's APIs within QML or namespaces having limited access to
[_cerrno_][errno]. Note, some non-POSIX platforms may not support few errno values (e.g. Windows).
Developers are discouraged from using raw errno values as they may vary depending on platforms.

| Constant                    | Value             | Description                          |
| :-------------------------- | :---------------- | :----------------------------------- |
| `FileMan::Success`          | `0`               | No error occurred                    |
| `FileMan::Busy`             | `EBUSY`/`16`      | Device or resource busy              |
| `FileMan::Canceled`         | `ECANCELED`/`125` | Further operations canceled          |
| `FileMan::Exists`           | `EEXIST`/`17`     | File exists                          |
| `FileMan::FileTooBig`       | `EFBIG`/`27`      | File is too large                    |
| `FileMan::Invalid`          | `EINVAL`/`22`     | Provided argument or file is invalid |
| `FileMan::IOError`          | `EIO`/`5`         | Input/output error                   |
| `FileMan::Loop`             | `ELOOP`/`40`      | Too many levels of symbolic links    |
| `FileMan::NotADir`          | `ENOTDIR`/`20`    | Not a directory                      |
| `FileMan::NotFound`         | `ENOENT`/`2`      | No such file or directory            |
| `FileMan::NotSupported`     | `EOPNOTSUPP`/`95` | Operation not allowed or supported   |
| `FileMan::NotUnique`        | `ENOTUNIQ`/`76`   | File not uniquely linked in store    |
| `FileMan::PermissionDenied` | `EPERM`/`1`       | Operation not permitted              |
| `FileMan::TryAgain`         | `EAGAIN`/`11`     | Operation temporarily failed         |

<div><a id="rm-file-type"></a></div>
<div><a name="rm-file-type"></a></div>

### rm_file_t

Type class holding properties _status_l_ and _status_b_:

| Property | Type |
| -------- | ---- |
| status_l | int  |
| status_b | int  |

<!-- markdownlint-disable no-duplicate-heading -->

#### Access Functions

<!-- markdownlint-enable -->

| Type                 | Method                                  |
| -------------------- | --------------------------------------- |
| int<br>`[nodiscard]` | status_l()                              |
| void                 | setStatus_l(int status_l) `[since 3.0]` |
| int<br>`[nodiscard]` | status_b()                              |
| void                 | setStatus_b(int status_b) `[since 3.0]` |

## Member Function Documentation

<div><a id="fileman-constructor-1"></a></div>
<div><a name="fileman-constructor-1"></a></div>

### FileMan([QObject][qobject] \*parent = nullptr) `[explicit]`

Creates the _FileMan_ singleton instance.

<div><a id="fileman-constructor-2"></a></div>
<div><a name="fileman-constructor-2"></a></div>

### FileMan(const [QDir][qdir] &storeDir, [QObject][qobject] \*parent = nullptr) `[explicit]`

Creates the _FileMan_ singleton instance, with application store at _storeDir_. Note that the
directory specified by _storeDir_ should be an absolute path, otherwise the store may end up under
current working directory.

<div><a id="fileman-constructor-3"></a></div>
<div><a name="fileman-constructor-3"></a></div>

### FileMan(const [QString][qstring] &appName, [QObject][qobject] \*parent = nullptr) `[explicit]` `[since 3.0]`

Overload of [FileMan()](#fileman-constructor-2), with the application name specified in _appName_.
This only affects the trailing part of the application store location, but does not modify or acts
as a replacement of the application name as set by `QCoreApplication::setApplicationName()` itself.

<div><a id="fileman-constructor-4"></a></div>
<div><a name="fileman-constructor-4"></a></div>

### FileMan(const [QString][qstring] &orgName, const [QString][qstring] &appName, [QObject][qobject] \*parent = nullptr) `[since 3.0]`

Overload of [FileMan()](#fileman-constructor-2), with both the application name and organization
name specified in _appName_ and _orgName_. This only affects the trailing part of the application
store location, but does not modify or acts as a replacement of the application name or organization
name as set by `QCoreApplication::setApplicationName()` or `::setOrganizationName()` itself.

<div><a id="fileman-destructor"></a></div>
<div><a name="fileman-destructor"></a></div>

### ~FileMan()

Destroys the _FileMan_ singleton instance.

<div><a id="addfile-1"></a></div>
<div><a name="addfile-1"></a></div>

### [add_file_t](#add-file-type) FileMan::addFile(_const_ [QUrl][qurl] _&fileUrl_, bool overwrite, [FileMan::Categories](#category-enum) _categories_ = FileMan::All) `[since 3.0]`

Adds the file from _fileUrl_ to the store with an optional _categories_ specified. Overwrites
existing entry if _overwrite_ is specified. The URL must follow local file specifiers:
`file:///path/to/myfile.img` for absolute or `file:myfile.img` for relative path to `myfile.img`.
Returns a [add_file_t](#add-file-type) struct:

| Member (Type)               | Value                                    |
| --------------------------- | ---------------------------------------- |
| `status()` (int)            | 0/`errno`                                |
| `location()` ([QUrl][qurl]) | `<fully_qualified_url_to_symlink>`/empty |
| `id()` ([QString][qstring]) | `<checksum hash>`/empty                  |

Failed returns follow [`errno`][errno] values. On success (`status() == 0`), a path to the symlink
in store and the checksummed hash as the file's ID is returned. For the example above, `location()`
holds `file:///home/user/.local/share/<APPNAME>/store/audios/MyAudio.ogg` and `id()` provides
`64988b348af3acf583d57260b44528f85da378b4dbcf4e55d36f9fac420a0ccd`. Unsuccessful operations return
corresponding non-zero `errno` value for `status()`, an empty URL for `location()` and empty string
for `id()`.

OR-ing [FileMan::Categories](#category-enum) to _categories_ adds _fileUrl_ to the first matching
category flag, even if it matches multiple categories. _categories_ can also be omitted, in which
case, _fileUrl_ is matched against all valid categories. Due to this ambiguity, it is recommended to
explicitly specify _categories_ as narrowly as possible.

For maximum efficiency, FileMan inherently attempts to _reflink_ the added file on filesystems that
support it. A storage free-space check is always performed _before_ writes initiate to predict and
prevent out-of-space failures.

Duplicate detection is performed on the hashed blobs, _not_ on the symlink names. If _overwrite_ is
set to false and attempt is made to add a file with a digest that already exists in the store, the
operation fails unambiguously - returning the path to the existing symlink if it is still linked, or
safely reuses the orphaned blob if it has lost its symlink. In either case, [errno][errno] `EEXIST`
is returned. However, it is still possible to add entirely different files that happen to share the
_same filename_ (i.e., different digests, but a naming conflict with an existing symlink) regardless
of _overwrite_ state. In such cases, FileMan gracefully resolves the conflict by appending a
closest-vacant numbered extension to the new symlink. For example, if `MyAudio.ogg` already exists
in the store, a new, inherently different `MyAudio.ogg` file will be safely placed as
`MyAudio-1.ogg`, while the underlying hashed blob logic remains untouched. The blob to symlink
relation is thus exactly one-one and unique.

When _overwrite_ is set to true, any duplicates in the store will be overwritten - the blob-symlink
pair or for orphans, the blob only will be removed first then added anew. If the blob could not be
removed, the situation is considered critical and the whole adding operation is aborted. If the
symlink could not be removed however, a renamed symlink is used as a workaround of the failure.

<!-- markdownlint-disable no-blanks-blockquote -->

> [!Note]
>
> - Symlinks are "shortcuts" on Windows, that is, they bear `.lnk` extension on the platform
> - Operations will continue even if storage free-space checks fail

> [!Warning]
>
> addFile() relies entirely on how [QMimeDatabase][qmimedb] recognizes the file. On some
> distributions, Qt's mime database is known to provide generic mime types even for obvious files in
> which case addFile() fails. This is an ongoing issue, if you encounter this, please do file a bug
> report.

> [!Important]
>
> addFile() does NOT handle cases where the existing blob may not be a valid file at all,
> potentially inhibiting adding files bearing the same digest further. This is intended by design to
> avoid unnecessary performance impact on heavy side validation-tasks. Although this is rarely ever
> likely, the situation can be resolved by calling [refreshStore()](#refreshstore).

<!-- markdownlint-enable -->

Example:

```cpp
//  MIT License : Lotconn 2026

#include <QtCoreApplication>
#include <QTextStream>
#include <FileMan>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("myapp");
    QTextStream in(stdin);
    FileMan fileman;

    while (!in.atEnd()) {

        qInfo() << "Provide file URL:";

        QString fileUrl = in.readLine().trimmed();

        if (fileUrl.isEmpty())
          continue;

        // omitting `categories` means `fileUrl` is matched against all known categories
        const add_file_t result = fileman.addFile(QUrl{ fileUrl }, true);

        qInfo() << "Program returned saved path:" << result.location();
        qInfo() << "Program returned id:" << result.id();

        if (int e = result.status(); e != 0)
          return e;
    }

    return 0;
}
```

<div><a id="addfile-2"></a></div>
<div><a name="addfile-2"></a></div>

### [add_file_t](#add-file-type) FileMan::addFile(_const_ [QUrl][qurl] _&fileUrl_, [FileMan::Categories](#category-enum) _categories_ = FileMan::All)

Overload of [addFile()](#addfile-1) retaining the API and behavior before v3.0. Implies _overwrite =
false_.

<div><a id="categorydir"></a></div>
<div><a name="categorydir"></a></div>

### `[[nodiscard]]` [QString][qstring] FileMan::categoryDir([FileMan::Category](#category-enum) category) _const_ `[since 3.0]`

Returns a [QString][qstring] containing the absolute directory of _category_ items in store.
`FileMan::All` is a composite _flag_ and therefore not supported. String is empty on invalid
category.

| _category_ [FileMan::Category](#category-enum) | Output ([QString][qstring])           |
| ---------------------------------------------- | ------------------------------------- |
| `FileMan::Audio`                               | `<AppLocalDataLocation>/store/audios` |
| `FileMan::Image`                               | `<AppLocalDataLocation>/store/images` |
| `FileMan::Video`                               | `<AppLocalDataLocation>/store/videos` |

<div><a id="filename"></a></div>
<div><a name="filename"></a></div>

### [QString][qstring] FileMan::fileName(_const_ [QUrl][qurl] &fileURL) `[static]` `[since 3.0]`

Extracts and returns the filename component from _fileUrl_. Returns an empty string if _fileURL_
ends with a trailing `/` or if resolution fails for any other reason.

```cpp
QString fileUrl = "file:///absolute/path to/.my/audio.ogg";
QString folderUrl = "file:///absolute/path to/.my/";

qInfo() << "FileName:" << fileman.fileName(QUrl{ fileUrl });
qInfo() << "FileName:" << fileman.fileName(QUrl{ folderPath });
// Output:
// FileName: "audio.ogg"
// FileName: ""
```

<div><a id="from-string-to-category"></a></div>
<div><a name="from-string-to-category"></a></div>

### [FileMan::Category](#category-enum) FileMan::fromStringToCategory(const [QString][qstring] &categoryName) `[static]` `[since 3.0]`

Returns the corresponding [FileMan::Category](#category-enum) for the provided _categoryName_ or
`FileMan::None` if _categoryName_ is not one of the supported or empty.

| _categoryName_ ([QString][qstring]) | Output [FileMan::Category](#category-enum) |
| ----------------------------------- | ------------------------------------------ |
| `Audio`                             | `FileMan::Audio`                           |
| `Image`                             | `FileMan::Image`                           |
| `Video`                             | `FileMan::Video`                           |

<div><a id="from-string-to-error"></a></div>
<div><a name="from-string-to-error"></a></div>

### [FileMan::Error](#error-enum) FileMan::fromStringToError(const [QString][qstring] &errorName) `[static]` `[since 3.0]`

Returns the corresponding [FileMan::Error](#error-enum) for the provided _errorName_ or
`Error::Success` if _errorName_ is not a valid string. _errorName_ should be the key name for the
enum, e.g. providing `Exists` returns `FileMan::Exists`.

<div><a id="instance"></a></div>
<div><a name="instance"></a></div>

### FileMan &FileMan::instance() `[static]`

Returns a reference to the already-constructed _FileMan_ singleton instance. Meant for code that has
no reference to the instance handed to it - e.g. a helper function, a slot, or a different
translation unit entirely - and would otherwise need _FileMan_ passed down or made global just to
reach it. Aborts fatally if called before any _FileMan_ instance has been constructed.

```cpp
// A cleanup routine connected elsewhere, with no `fileman` in scope
void onAboutToQuit() {

  // FileMan localMan; <--- WRONG! will abort fatally

  const int status = FileMan::instance().refreshStore(); // <-- CORRECT, call the already available
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

<div><a id="instance-exists"></a></div>
<div><a name="instance-exists"></a></div>

### bool &FileMan::instanceExists() `[static]` `[since 3.0]`

Returns whether the singleton instance exists. True if does, false if it does not.

<div><a id="refreshstore"></a></div>
<div><a name="refreshstore"></a></div>

### int FileMan::refreshStore()

Executes comprehensive cleanup routines on the store. This includes pruning stale or invalid files
(such as dangling symlinks, corrupted data, or orphaned blobs) and attempting generic fixes on store
directories (e.g., resolving permission errors, or correcting non-existent or improperly symlinked
directories). When pruning, symlinks bearing the same target are all removed except for the oldest
one. Finally, it completely rebuilds the in-memory store-state cache that other APIs frequently rely
upon. Returns 0 on success, `errno` on failure.

Because it acts as the authoritative consolidator of the store's state, _refreshStore()_ does not
rely on existing store-state caches for its operations - it start from a position of zero trust,
applying the strictest possible validation to every single file in the store. This validation logic
is effectively the union of the rigorous checks performed by both [_addFile()_](#addfile-1) and
[_rmFile()_](#rmfile-1).

Once housekeeping is completed successfully, FileMan will globally flag the store as trustworthy.
This allows subsequent API calls to safely skip redundant, heavy validations and instead rely on the
freshly rebuilt, in-cache store-state dictionaries for optimal performance. Highly recommended to be
exposed as an interactable state like [`addFile()`](#addfile-1) or [`rmFile()`](#rmfile-1).
Implementations should call the method at least once on process startup or before exit.

<div><a id="rmfile-1"></a></div>
<div><a name="rmfile-1"></a></div>

### [rm_file_t](#rm-file-type) FileMan::rmFile(_const_ [QUrl][qurl] &fileUrl)

Removes the file _fileUrl_ from the application store. _fileUrl_ must be a fully-qualified local URL
referring to a file in store. Removes both the referred file and the backing hashed blob. Returns a
[rm_file_t](#rm-file-type) struct:

| Member (Type)      | Values       |
| ------------------ | ------------ |
| `status_l()` (int) | 0/`errno`    |
| `status_b()` (int) | 0/-1/`errno` |

When processing a removal request, validations are always be performed on the _fileUrl_ before any
checks are made on the backing blob:

- **Link Validation (`status_l`):** This will return a non-zero _errno_ if the _fileUrl_ fails
  validation. This includes scenarios where the URL is not a path within the store, the file does
  not exist, is not a symlink, points to an invalid or external location, or if the symlink removal
  itself fails.
- **Blob Validation (`status_b`):** This will return `-1` if _rmFile()_ fails early (before blob
  checks are even reached, due to the link validation failure). It will return a non-zero positive
  _errno_ only if the backing hashed blob validations fail (for reasons similar to _addFile()_'s
  logic) or if the blob removal fails. If multiple symlinks in the store point to the same backing
  blob, `rmFile()` refuses to remove the blob but still removes the provided link (`status_l = 0`),
  setting `status_b` to `ENOTUNIQ`.

If all validations pass, FileMan proceeds to remove the requested file. Continuing our previous
example, passing the URL `file:///home/user/.local/share/myapp/store/audios/MyAudio.ogg` to
_rmFile()_ will erase the uniquely identified file from the store. Keen readers will recognize this
as a direct parallel to the _REST-member DELETE_ convention, and they would be entirely correct.

FileMan will safely remove valid symlink files that the _fileUrl_ refers to within the categorized
directories, but it will strictly refuse if the _fileUrl_ points to any location outside the
categorized store.

Incomplete removals may be possible, for example, the backing blob might be successfully removed,
but the symlink removal could fail due to a sudden permission change or lock. To minimize chances of
"blob orphanage", the backing hashed blob is always targeted for removal first. If the symlink
removal subsequently fails, it leaves behind a dangling symlink. This is an acceptable, fail-safe
state: FileMan is designed to detect and safely clean up these dangling symlinks on the next API
call, provided the caller checks the error returns appropriately and retries if necessary.

<div><a id="rmfile-2"></a></div>
<div><a name="rmfile-2"></a></div>

### [rm_file_t](#rm-file-type) FileMan::rmFile(_const_ [QString][qstring] &fileId) `[since 3.0]`

Overload of [rmFile()](#rmfile-1). Removes the blob matching _fileId_ and its associated symlinks
from the application store. _fileId_ must be a valid checksum hash referring to a blob in store, as
returned by [addFile()](#addfile-1). Returns a [rm_file_t](#rm-file-type) struct:

| Member (Type)      | Values       |
| ------------------ | ------------ |
| `status_l()` (int) | 0/-1/`errno` |
| `status_b()` (int) | 0/`errno`    |

When processing a removal request, validations are performed first on the blob matching _fileId_
before any checks are made on the associated symlinks:

- **Blob Validation (`status_b`):** This will return a non-zero positive _errno_ only if the
  matching hashed blob validations fail (for reasons similar to _addFile()_'s logic) or if the blob
  removal fails. If multiple symlinks in the store point to the same backing blob, `rmFile()` will
  remove the blob and its associated links.
- **Link Validation (`status_l`):** This will return `-1` if _rmFile()_ fails early (before link
  checks are even reached, due to the blob validation failure). Will return a non-zero _errno_ if
  the links fail validations. This includes scenarios where the symlink is not a path within the
  store, the file does not exist, is not a symlink, points to an invalid or external location, or if
  the symlink removal itself fails.

If all validations pass, FileMan proceeds to remove the requested file. Continuing our previous
example, passing the ID `64988b348af3acf583d57260b44528f85da378b4dbcf4e55d36f9fac420a0ccd` to
_rmFile()_ will safely erase the uniquely identified blob and its associated valid link(s) from the
store, but will strictly refuse if the links point to any location outside the categorized store.

Incomplete removals may be possible, for example, the backing blob might be successfully removed,
but the symlink removal could fail due to a sudden permission change or lock. To minimize chances of
"blob orphanage", the backing hashed blob is always targeted for removal first, and aborts further
procedures on failure. If the symlink removal subsequently fails, it leaves behind a dangling
symlink. This is an acceptable, fail-safe state: FileMan is designed to detect and safely clean up
these dangling symlinks on the next API call, provided the caller checks the error returns
appropriately and retries if necessary.

<div><a id="storedir"></a></div>
<div><a name="storedir"></a></div>

### `[[nodiscard]]` [QString][qstring] FileMan::storeDir() _const_ `[since 3.0]`

Returns a [QString][qstring] containing the absolute directory of `<AppLocalDataLocation>/store`.
String is never empty.

<div><a id="tostring-1"></a></div>
<div><a name="tostring-1"></a></div>

### [QString][qstring] FileMan::toString([FileMan::Category](#category-enum) category) `[static]` `[since 3.0]`

Returns a [QString][qstring] containing the name of _category_ or empty if _category_ is an invalid
key in the enum. `FileMan::All` is a composite _flag_ and therefore not supported.

| _category_ [FileMan::Category](#category-enum) | Output ([QString][qstring]) |
| ---------------------------------------------- | --------------------------- |
| `FileMan::Audio`                               | `Audio`                     |
| `FileMan::Image`                               | `Image`                     |
| `FileMan::Video`                               | `Video`                     |

<div><a id="tostring-2"></a></div>
<div><a name="tostring-2"></a></div>

### [QString][qstring] FileMan::toString([FileMan::Error](#error-enum) error) `[static]` `[since 3.0]`

Returns a [QString][qstring] containing the string of the _error_ key or empty if _error_ is an
invalid key in the enum. For example, providing `FileMan::Exists` returns `Exists` as string. The
string is empty if _error_ is an invalid key in FileMan::Error.

<div><a id="integration"></a></div>
<div><a name="integration"></a></div>

## Integration & Usage

<div><a id="cpp-integration"></a></div>
<div><a name="cpp-integration"></a></div>

### Qt/C++ Integration Guide

Qt/C++ applications can integrate FileMan in just _four_ simple steps:

#### 1. CMake Integration

(Pick one from step 1):

<div><a id="include-fetchcontent"></a></div>
<div><a name="include-fetchcontent"></a></div>

##### 1.1 Quick Integration with CMake `FetchContent` module

If you're using CMake as your build system, `FetchContent` module makes it very easy to include
FileMan as a third-party lib.

```cmake
include(FetchContent)

FetchContent_Declare(FileMan
  GIT_REPOSITORY https://gitlab.com/lotconn/fileman
  GIT_TAG v3.0.0 # Or use GIT_COMMIT <full commit shasum>
  GIT_SHALLOW TRUE
)
# Uncomment this to build statically
# set(FM_BUILD_AS "STATIC" CACHE STRING "Set FileMan build type" FORCE)
FetchContent_MakeAvailable(FileMan)

target_link_libraries(${PROJECT_NAME} PRIVATE Lotconn::FileMan)

# lld is required for clang, can be omitted if using gcc instead
target_link_options(${PROJECT_NAME} PRIVATE $<$<CXX_COMPILER_ID:Clang>:-fuse-ld=lld>)
```

<div><a id="include-submodule"></a></div>
<div><a name="include-submodule"></a></div>

##### 1.2 Including as a submodule

This is a very common way of integrating third-party libraries within a project. Decide where to add
the FileMan submodule first. For our guide here, we'll add it under `libs/` from the project root.
Whichever location you choose, just make sure the trailing directories are `Lotconn/FileMan/`
complying with Qt's expected URI translated lookup:

```bash
git clone --depth 1 --branch v3.0.0 --recurse-submodules https://gitlab.com/lotconn/fileman libs/Lotconn/FileMan
git submodule add https://gitlab.com/lotconn/fileman libs/Lotconn/FileMan
```

On your project's main `CMakeLists` just replace the `FetchContent` block with this: [^1]

```cmake
# everything before FetchContent_Decalre()

# Uncomment this to build statically
# set(FM_BUILD_AS "STATIC" CACHE STRING "Set FileMan build type" FORCE)
add_subdirectory(libs/Lotconn/FileMan)

# everything after FetchContent_MakeAvailable()
```

<div><a id="include-system-installed"></a></div>
<div><a name="include-system-installed"></a></div>

##### 1.3 Using system-installed instance

> [!Important]
>
> This requires FileMan to be [built & installed](CONTRIBUTING.md/#building) on your running system
> first.

Add these lines only:

```cmake
find_package(FileMan REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE Lotconn::FileMan)
```

For statically linking however, make sure the system-installed instance has been built
[statically](CONTRIBUTING.md/#available-build-flags).

#### 2. Include the Header

Include the public routing header in your C++ source files:

```cpp
#include <Lotconn/FileMan/FileMan>
// #include <FileMan> also works
```

#### 3. Instantiate After Setting the Application Name

FileMan requires the application name to resolve the store path. Always set it before instantiation:

```cpp
QCoreApplication app(argc, argv);
QCoreApplication::setApplicationName("myapp");

FileMan myman;
```

#### 4. Use the Public API

Call instance or static methods as needed:

```cpp
// Instance member example
const auto result = myman.addFile(QUrl{ "file:///home/user/Music/MyAudio.ogg" });

// Static member example
const auto storeDir = FileMan::storeDir();
```

_That's it!_

<div><a id="qml-integration"></a></div>
<div><a name="qml-integration"></a></div>

### QML Integration Guide

For QML integrations, FileMan can be used as a QML module and explicit instantiation from the C++
backend is not necessary.

> [!Note]
>
> While it is technically possible to embed FileMan as a context property in QML's root namespace,
> this approach is discouraged. The recommended and most robust method is to use FileMan as a proper
> module.

Integrating FileMan into a QML project is easily done in just _three_ steps:

#### 1. Configure the QML Module in CMake

Continuing from the previous CMake excerpt, add the QML module configuration to your application's
main `CMakeLists.txt`:

```cmake
# FetchContent(), add_subdirectory() or find_package() from previous excerpt

# The following goes after qt_add_executable()
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

                  DEPENDENCIES Lotconn.FileMan # <-- Crucial: Links the QML module to FileMan
                )

target_link_libraries(${PROJECT_NAME} PRIVATE Lotconn::FileMan)
```

For static building, include the directives to build FileMan as
[static](CONTRIBUTING.md/#available-build-flags) as previously and link against the FileMan plugin
instead:

```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE FileManplugin)
# OR, for system-installed instance
target_link_libraries(${PROJECT_NAME} PRIVATE Lotconn::FileManplugin)
```

Add a QML module import macro on your `main.cpp`. For QML-only implementations, this is the only
inclusion necessary:

```cpp
#include <QtQml/QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(Lotconn_FileManPlugin)
```

#### 2. Import the Module

Import the module at the top of any QML file where you intend to use it:

```qml
import Lotconn.FileMan
```

#### 3. Call and Operate

You can now directly call and operate on any member FileMan provides, utilizing its singleton nature
natively within QML:

```qml
const result = FileMan.addFile(fileUrl)
```

_That's it!_

> [!Important]
>
> FileMan's types and members are accessible only inside "FileMan" scope. Qt doesn't allow importing
> the module on a custom namespace.

Refer to the [QML example](examples/fileman-qml/) for a basic working implementation.

> [!Important]
>
> Example QML apps in the project built statically inside the container are not portable, they fail
> to run due to missing QT_PRIVATE_API, and won't run properly outside the container itself. Build
> these static apps on your host instead if you want to run them. Alternatively, build them on a
> statically built Qt system for them to be truly portable.

## Implementations

- **Rukn**
  - Homepage: <https://lotconn.org>
  - VCS Browser: <https://gitlab.com/lotconn/rukn>

## Contributing

This project uses CMake as the build environment. Minimum required is 3.24 with highest policy set
to 4.4. Clang with LLVM, GCC, MingW and MSVC are all supported. See
[CONTRIBUTING.md](./CONTRIBUTING.md) for further details about building, available build flags and
contribution guidelines & policies.

## License

This project is licensed under MIT, copyright Lotconn, 2026.

## Acknowledgements

### Libraries used in this project

- **Qt** by The Qt Company
  - Project Implementation: UI and core code
  - License: LGPL v3
  - Homepage: <https://www.qt.io/>
  - Source: <https://code.qt.io/cgit/>

### Contributors

- **Project Author:** Hasan Esa [@HasanEsa](https://gitlab.com/HasanEsa)

---

[^1]: For full reference on `qt_add_qml_module()`: <https://doc.qt.io/qt-6/qt-add-qml-module.html>

[errno]: https://en.cppreference.com/cpp/header/cerrno
[qdir]: https://doc.qt.io/qt-6/qdir.html
[qmimedb]: https://doc.qt.io/qt-6/qmimedatabase.html
[qobject]: https://doc.qt.io/qt-6/qobject.html
[qstring]: https://doc.qt.io/qt-6/qstring.html
[qurl]: https://doc.qt.io/qt-6/qurl.html
[string]: https://doc.qt.io/qt-6/qml-string.html
[url]: https://doc.qt.io/qt-6/qml-url.html
