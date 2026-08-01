# Migration Guide: v1.0.1 to v2.0.0

This guide outlines the necessary changes to migrate your project from FileMan v1.0.1 to v2.0.0. This release involves significant breaking changes regarding namespaces, API signatures, and build configurations.

## 1. Namespace & Import Updates

All references to `HasanEsa` must be replaced with `Lotconn`.

### C++ Includes

**Old:**

```cpp
#include <HasanEsa/FileMan/fileman.hpp> // or #include <fileman.hpp>
```

**New:**

```cpp
#include <Lotconn/FileMan/fileman>
```

### QML Imports

**Old:**

```qml
import HasanEsa.FileMan
// or
import HasanEsa.FileMan.Enums
```

**New:**

```qml
import Lotconn.FileMan
// or
import Lotconn.FileMan.Enums
```

### CMake Configuration

**Old:**

```cmake
target_link_libraries(myapp PRIVATE HasanEsa::FileMan)

qt_add_qml_module(myapp DEPENDENCIES HasanEsa.FileMan)
```

**New:**

```cmake
target_link_libraries(myapp PRIVATE Lotconn::FileMan)

qt_add_qml_module(myapp DEPENDENCIES Lotconn.FileMan)
```

If using static linking:

```cmake
target_link_libraries(myapp PRIVATE Qt::Qml Lotconn::FileManplugin)
```

In main.cpp:

```cpp
#include <QtQml/QQmlExtensionPlugin> Q_IMPORT_QML_PLUGIN(Lotconn_FileManPlugin)
```

## 2. API Signature Changes

### `addFile()` Return Type

The function no longer returns a `QVariantMap`. It now returns a strongly typed `add_file_t` struct.

**Old (v1.0.1):**

```cpp
QVariantMap result = fileman.addFile(url, FileMan::Audio);

if (result["error"].toInt() != 0)
  qWarning() << "Error:" << result["error"];
else
  qInfo() << "Saved:" << result["path"].toString();
```

**New (v2.0.0):**

```cpp
add_file_t result = fileman.addFile(url, FileMan::Audio);

if (result.status() != 0)
  qWarning() << "Error:" << result.status();
else
  qInfo() << "Saved:" << result.location().toDisplayString();
```

### `rmFile()` Overhaul

The signature has changed significantly. It now accepts a `QUrl` pointing to the file in the store and returns `rm_file_t` with separate status codes for symlink and blob removal.

**Old (v1.0.1):**

```cpp
// Arguments: fileName (string), category (flag)
int status = fileman.rmFile("my_audio.ogg", FileMan::Audio);
if (status != 0) {
  /* handle error */
}
```

**New (v2.0.0):**

```cpp
// Argument: Fully qualified URL to the symlink in store
// Example: file:///home/user/.local/share/myapp/audio/my_audio.ogg
rm_file_t status = fileman.rmFile(QUrl("file:///path/to/store/audio/my_audio.ogg"));

// Check symlink removal status
if (status.status_l() != 0)
  qWarning() << "Symlink removal failed:" << status.status_l();

// Check hash blob removal status (-1 means early failure, 0 success, >0 error)
if (status.status_h() != 0)
  qWarning() << "Blob removal failed/warning:" << status.status_h();
```

### `storeHouseKeeping()` Renamed

The housekeeping function has been renamed to `refreshStore()` and now returns `int` (errno) directly.

**Old:**

```cpp
int err = fileman.storeHouseKeeping();
```

**New:**

```cpp
int err = fileman.refreshStore();
```

### Static vs. Instance Access

`getDataDir()` is no longer static. You must instantiate `FileMan` or use the singleton accessor.

**Old:**

```cpp
QString dir = FileMan::getDataDir();
```

**New:**

```cpp
// Option A: Via instance FileMan fileman;
QString dir = fileman.getDataDir();

// Option B: Via singleton accessor (after instantiation elsewhere)
QString dir = FileMan::instance().getDataDir();
```

## 3. Build System Updates

Ensure your `CMakeLists.txt` meets the new minimum requirements:

- **CMake:** Minimum version 3.16, policies up to 4.4.
- **Qt:** Strict mode set to 6.8 (`QT_ENABLE_STRICT_MODE_UP_TO=0x060800`).
- **Compiler:** Clang/LLVM with Ninja is strongly recommended.

If you were manually setting compiler flags, note that v2.0.0 now enforces security hardening flags automatically (Fortify, Stack Protector, CET on x86_64).

## 4. Singleton Behavior

FileMan is strictly a singleton.

- **C++:** Instantiating `FileMan` twice will cause a fatal abort. Use `FileMan::instance()` to access the existing instance.
- **QML:** The singleton is auto-managed. Do not attempt to manually construct it in C++ if you intend to use it purely via QML context properties unless you need to embed it in the root context.

## 5. Checklist for Migration

- [ ] Replace all `HasanEsa` occurrences with `Lotconn` in code and CMake.
- [ ] Update `#include` directives to `<Lotconn/FileMan/fileman>`.
- [ ] Refactor `addFile()` calls to use `result.status()` and `result.location()`.
- [ ] Refactor `rmFile()` calls to accept `QUrl` and check `status_l()`/`status_h()`.
- [ ] Rename `storeHouseKeeping()` to `refreshStore()`.
- [ ] Update `getDataDir()` calls to use an instance or `FileMan::instance()`.
- [ ] Verify CMake policies and Qt strict mode settings.
- [ ] Run `clang-tidy` to ensure compliance with new linting rules.

## 6. Rollback

If you encounter critical issues, you can revert to v1.0.1 by checking out the tag `v1.0.1` and updating your dependencies accordingly. Note that v1.0.1 uses the `HasanEsa` namespace and the older `QVariantMap` API.
