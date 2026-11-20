# Migration Guide: v2.x to v3.x

This guide outlines the necessary changes to migrate your project from FileMan v2.x to v3.x. This
release involves significant breaking changes regarding namespaces, API signatures, and build
configurations.

## 1. Source Usage Updates

All references to `fileman` router must be replaced with `FileMan`.

### C++ Includes

**Old:**

```cpp
#include <Lotconn/FileMan/fileman>
// Or #include <fileman>
```

**New:**

```cpp
#include <Lotconn/FileMan/FileMan>
// Or #include <FileMan>
```

### Enum Usages, QML + C++

**Old:**

```cpp
const auto result = myman.addFile(fileUrl, FileMan::Images);
```

```qml
const result = FileMan.addFile(fileUrl, FileMan.Images)
```

**New:**

```cpp
const auto result = myman.addFile(fileUrl, FileMan::Categories::Images);
// Use Category where the enum form is required
```

```qml
const result = FileMan.addFile(fileUrl, FileMan.Categories.Images)
// Use Category where the enum form is required
```

## 2. Build System Updates

Ensure your `CMakeLists.txt` meets the new minimum requirements:

- **CMake:** Minimum version 3.24, policies up to 4.4.
- **Qt:** Strict mode set to 6.12 (`QT_ENABLE_STRICT_MODE_UP_TO=0x061200`).

## 3. Checklist for Migration

- [ ] Update `#include` directives to `<Lotconn/FileMan/FileMan>`.
- [ ] Replace all instances of unscoped `FileMan::Categories/Category` flag/enum usages to scoped
- [ ] Verify CMake policies and Qt strict mode settings.
- [ ] Run `clang-tidy` to ensure compliance with new linting rules.

## 4. Rollback

If you encounter critical issues, you can revert to v2.x by checking out the last v2 tag `v2.0.2`
and updating your dependencies accordingly.
