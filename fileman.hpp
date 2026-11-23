#pragma once

#include <QDir>
#include <QObject>
#include <QQmlEngine>
#include <QSharedDataPointer>

#if defined(FM_STATIC_LIB)
#  define FM_SYM_ATTR
#elif defined(FM_LIB_BUILD)
#  define FM_SYM_ATTR Q_DECL_EXPORT
#else
#  define FM_SYM_ATTR Q_DECL_IMPORT
#endif

class AddFileType;
class RmFileType;

struct alignas(8) FM_SYM_ATTR add_file_t // NOLINT(*magic-numbers)
{
    Q_GADGET
    Q_PROPERTY(int status READ status CONSTANT)
    Q_PROPERTY(QUrl location READ location CONSTANT)
    Q_PROPERTY(QString id READ id CONSTANT)
    QML_ANONYMOUS

public:
    add_file_t();
    add_file_t(int status, const QUrl &location, const QString &id);
    ~add_file_t();

    [[nodiscard]] auto status() const -> int;
    [[nodiscard]] auto location() const -> QUrl;
    [[nodiscard]] auto id() const -> QString;
    [[nodiscard]] auto isNull() const noexcept -> bool;

    add_file_t(const add_file_t &);
    add_file_t(add_file_t &&) noexcept;
    add_file_t &operator=(const add_file_t &);
    add_file_t &operator=(add_file_t &&) noexcept;

private:
    QExplicitlySharedDataPointer<AddFileType> d_ptr;
};

struct alignas(8) FM_SYM_ATTR rm_file_t // NOLINT(*magic-numbers)
{
    Q_GADGET
    Q_PROPERTY(int status_l READ status_l CONSTANT)
    Q_PROPERTY(int status_b READ status_b CONSTANT)
    QML_ANONYMOUS

public:
    rm_file_t();
    rm_file_t(int status_l, int status_b);
    ~rm_file_t();

    [[nodiscard]] auto status_l() const -> int;
    [[nodiscard]] auto status_b() const -> int;
    [[nodiscard]] auto isNull() const noexcept -> bool;

    rm_file_t(const rm_file_t &);
    rm_file_t(rm_file_t &&) noexcept;
    rm_file_t &operator=(const rm_file_t &);
    rm_file_t &operator=(rm_file_t &&) noexcept;

private:
    QExplicitlySharedDataPointer<RmFileType> d_ptr;
};

class FileManPrivate;

class FM_SYM_ATTR FileMan : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")
    Q_DECLARE_PRIVATE(FileMan)
    Q_DISABLE_COPY_MOVE(FileMan)
    QML_ELEMENT
    QML_SINGLETON

    inline static FileMan *s_singletonInstance{ };
    FileManPrivate *const  d_ptr;

public:
    explicit FileMan(QObject *parent = nullptr);
    explicit FileMan(const QDir &storeDir, QObject *parent = nullptr);
    explicit FileMan(const QString &appName, QObject *parent = nullptr);
    FileMan(const QString &orgName, const QString &appName, QObject *parent = nullptr);
    ~FileMan() override;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    //  Type/Enum definitions
    ////////////////////////////////////////////////////////////////////////////////////////////////

    enum Category : quint8 {
        None  = 0x00,
        Audio = 0x01,
        Image = 0x02,
        Video = 0x04,
        All   = Audio | Image | Video
        // ceiling is 0xFF
    };
    Q_DECLARE_FLAGS(Categories, Category)
    Q_FLAG(Categories)

    enum Error : quint8 {
        Success          = 0,
        Busy             = EBUSY,
        Exists           = EEXIST,
        FileTooBig       = EFBIG,
        Invalid          = EINVAL,
        IOError          = EIO,
        Loop             = ELOOP,
        NotFound         = ENOENT,
        NotADir          = ENOTDIR,
        NotUnique        = ENOTUNIQ,
        NotSupported     = EOPNOTSUPP,
        PermissionDenied = EPERM,
        TryAgain         = EAGAIN
        // ceiling is 0xFF
    };
    Q_ENUM(Error)

    ////////////////////////////////////////////////////////////////////////////////////////////////
    //  Function definitions
    ////////////////////////////////////////////////////////////////////////////////////////////////

    Q_INVOKABLE auto addFile(const QUrl &fileUrl, bool overwrite,
                             Categories categories = Category::All) -> add_file_t;
    Q_INVOKABLE auto addFile(const QUrl &fileUrl, Categories categories = Category::All)
            -> add_file_t;
    Q_INVOKABLE [[nodiscard]] auto categoryDir(Category category) const -> QString;
    Q_INVOKABLE auto               refreshStore() -> int;
    Q_INVOKABLE auto               rmFile(const QUrl &fileUrl) -> rm_file_t;
    Q_INVOKABLE auto               rmFile(const QString &fileId) -> rm_file_t;
    Q_INVOKABLE [[nodiscard]] auto storeDir() const -> QString;

    Q_INVOKABLE static auto fileName(const QUrl &fileUrl) -> QString;
    Q_INVOKABLE static auto fromStringToCategory(const QString &categoryName) -> Category;
    Q_INVOKABLE static auto fromStringToError(const QString &errorName) -> Error;
    static auto             instance() -> FileMan &;
    static auto             instanceExists() -> bool;
    Q_INVOKABLE static auto toString(Category category) -> QString;
    Q_INVOKABLE static auto toString(Error error) -> QString;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FileMan::Categories)