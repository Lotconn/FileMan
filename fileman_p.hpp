#pragma once

#include <QHash>
#include <QLockFile>
#include <QLoggingCategory>
#include <QMutex>
#include <QReadWriteLock>
#include <QSaveFile>

#include <FileMan>

Q_DECLARE_LOGGING_CATEGORY(fileman)

#define logDebug    qCDebug(fileman).noquote() << __func__ << ": "
#define logInfo     qCInfo(fileman).noquote() << __func__ << ": "
#define logWarning  qCWarning(fileman).noquote() << __func__ << ": "
#define logCritical qCCritical(fileman).noquote() << __func__ << ": "
#define logFatal    qCFatal(fileman).noquote() << __func__ << ": "

// NOLINTBEGIN(*global-names-in-headers)
// Allows shorthand QStringLiteral operators. Included in this internal header for all sources to
// utilize
using Qt::StringLiterals::operator""_L1;
using Qt::StringLiterals::operator""_s;
// NOLINTEND(*global-names-in-headers)

class FileManPrivate
{
    explicit FileManPrivate(FileMan *q) : q_ptr{ q } { }
    ~FileManPrivate() = default;

    Q_DISABLE_COPY_MOVE(FileManPrivate)
    Q_DECLARE_PUBLIC(FileMan)

    FileMan *const q_ptr;

    using Category   = FileMan::Category;
    using Categories = FileMan::Categories;
    using Error      = FileMan::Error;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    //  Types //////////////////////////////////////////////////////////////////////////////////////

    // NOLINTBEGIN(*magic-numbers)
    struct alignas(8) pre_add_validate_t
    {
        int        status = { };
        Categories categories;
    };

    struct alignas(64) pre_rm_validate_t
    {
        int     status    = { };
        QString lnkEntry  = { };
        QString blobEntry = { };
    };

    struct alignas(32) rev_lookup_symlink_t
    {
        int         status = { };
        QStringList locations;
    };

    struct alignas(16) validate_store_t
    {
        int                  d_status = { };
        int                  h_status = { };
        QHash<Category, int> c_status;
    };
    // NOLINTEND(*magic-numbers)

    ////////////////////////////////////////////////////////////////////////////////////////////////
    //  Variables and objects //////////////////////////////////////////////////////////////////////

    // 64KiB is a good chunk size for almost all purposes including network files, although 1MiB
    // would've been more ideal for local disk files
    static constexpr qint32       ioChunkSize   = 65536;
    inline static const QMetaEnum metaErrorEnum = QMetaEnum::fromType<Error>();

    inline static const QHash<QString, Category> stringToCategory = {
        { u"Audio"_s, Category::Audio },
        { u"Image"_s, Category::Image },
        { u"Video"_s, Category::Video },
    };

    inline static const QHash<Category, QString> categoryToString = {
        { Category::Audio, u"Audio"_s },
        { Category::Image, u"Image"_s },
        { Category::Video, u"Video"_s },
    };

    inline static const QHash<Category, QString> sc_categoryToDirNames = {
        { Category::Audio, u"audios"_s },
        { Category::Image, u"images"_s },
        { Category::Video, u"videos"_s },
    };

    inline static const QHash<QString, Category> sc_dirNamesToCategory = {
        { u"audios"_s, Category::Audio },
        { u"images"_s, Category::Image },
        { u"videos"_s, Category::Video },
    };

    inline static const QString sc_blobDirName  = u"common"_s;
    inline static const QString sc_storeDirName = u"store"_s;

    QString                  storeDirPath;
    QString                  blobDirPath;
    QHash<Category, QString> categoryToDirs;
    QHash<QString, Category> dirsToCategory;
    QHash<QString, QString>  blobToLinkDict;
    QHash<QString, QString>  linkToBlobDict;
    QLockFile               *storeLock{ };
    QMutex                   storeMutex;
    std::atomic<bool>        storeManaged;
    QReadWriteLock           varLock{ QReadWriteLock::Recursive };

    ////////////////////////////////////////////////////////////////////////////////////////////////
    //  Functions //////////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Acquires explicit thread lock & lock on the store for serialized access and preventing
     * race conditions.
     *
     * @return int: 0 on success, errno on failure.
     */
    auto acquireStoreLock() noexcept -> int;

    /**
     * @brief File digest (SHA256) computer.
     *
     * @param path: Path to the file to be checksummed
     * @return QString: Unseparated 64-char string for the computed digest, empty otherwise.
     */
    static auto calcDigest(const QString &path) noexcept -> QString;

    /**
     * @brief Clears all entries in the dictionaries.
     */
    auto clearAllEntries() noexcept -> void;

    /**
     * @brief Compares two errno error values in terms of pre-defined severity grades.
     *
     * @return int: the severest between `error1` and `error2`, or `error` if both are equal.
     */
    static auto compErrnoSeverity(int error1, int error2) noexcept -> int;

    /**
     * @brief Concatenates two path components `comp1` and `comp2` and returns the combined clean
     * path with platform relevant directory separator. Never returns empty string.
     *
     * @param comp1: Leading path component
     * @param comp2: Trailing path component
     * @return QString: Concatenated path component
     */
    static auto createPathComp(const QString &comp1, const QString &comp2) noexcept -> QString;

    /**
     * @brief Checks `dirPath` for existence, symbolic linkage and appropriate permissions.
     *
     * @return int: 0 on success, `errno` on failure.
     */
    static auto dirHealth(const QString &dirPath) noexcept -> int;

    /**
     * @brief Check for available free space in `dir` for accomodating `file`.
     *
     * @return int: 0 on success, -1 if unable to check `dir` and ENOSPC if doesn't pass the check.
     */
    static auto diskFreeCheck(const QFileInfo &file, const QDir &dir) noexcept -> int;

    /**
     * @brief Dumps all entries in both the dictionaries to log. Nothing else.
     */
    auto dumpAllEntries() noexcept -> void;

    /**
     * @brief Maps common errno to a severity rank for comparison purposes. Higher = more severe.
     * Unlisted/unexpected errno (e.g. raw ec.value() from std::error_code) is treated as less
     * severe than ENOTDIR.
     */
    static auto errnoSeverity(int e) noexcept -> int;

    /**
     * @brief Attempts appropriate repairs on `dir` following the provided `error`.
     *
     * @param dir: Path to directory for repairing, relative to store directory
     * @param error: The specific error related to this directory needing fix
     * @param category: (optional) The category to look for `dir` in. Defaults to nothing.
     * @return bool: true if repairs were successful, false otherwise.
     */
    [[nodiscard]] auto fixDirHealth(const QString &dir, int error,
                                    Category category = Category::None) const noexcept -> bool;

    /**
     * @brief Initiates all paths and dictionaries related to category vs directory relations.
     *
     * @param storeDir: Root path of the store
     */
    auto initPathsAndDicts(const QDir &storeDir) noexcept -> void;

    /**
     * @brief Checks whether dictionary is empty or not.
     *
     * @param dict: The hash-dictionary to check
     * @return bool: true if `dict` is empty, false if not
     */
    auto isDictEmpty(const QHash<QString, QString> &dict) noexcept -> bool;

    /**
     * @brief Validate if `ch` is a hex character. No Qt-native methods exists as of now for this.
     *
     * @return bool: true if `ch` is a hex character, false otherwise.
     */
    static auto isHexDigit(QChar ch) noexcept -> bool;

    /**
     * @brief Looks-up the blob entry for `lnkEntry` in the in-memory hash-table.
     *
     * @param lnkEntry: Link to lookup with in the hash-table
     * @return QString: The blob entry referenced by `lnkEntry`. Empty if entry doesn't exists.
     */
    auto lookupBlobByLink(const QString &lnkEntry) noexcept -> QString;

    /**
     * @brief Looks-up the symlink entry for `blobName` in the in-memory hash-table.
     *
     * @param blobName: Blob to lookup in the hash-table
     * @return QString: The link entry referenced by `blobName`. Empty if entry doesn't exist
     */
    auto lookupLinkByBlob(const QString &blobName) noexcept -> QString;

    /**
     * @brief Isolates the first set `FileMan::Category` enum from possible combination of
     * `FileMan::Categories` flag in `categories`. Returns `Category::None` if nothing matched.
     */
    static auto lowestSetFlag(Categories categories) noexcept -> Category;

    /**
     * @brief Creates the full path to dataDir/common as required.
     *
     * @return bool: true if path successfully created or exists, false otherwise.
     */
    [[nodiscard]] auto mkDataDir() const noexcept -> bool;

    /**
     * @brief Overload of `mkDataDir()`. Creates the full path to `dataDir/<category>` as required.
     *
     * @param category: Category directory to create under `dataDir/`
     * @return bool: true if path successfully created or exists, false otherwise.
     */
    [[nodiscard]] auto mkDataDir(Category category) const noexcept -> bool;

    /**
     * @brief Validator function before adding `file` to store. Returns a `pre_add_validate_t`
     * struct containing `{int, FileMan::Categories}` members.
     *
     * @param file: The file (object) to validate against.
     * @return pre_add_validate_t: `{0, FileMan::<valid_categories>}` if successful, `{errno,
     * FileMan::None}` if otherwise. `errno` maybe one of `{EFBIG, EINVAL, ELOOP, EOPNOTSUPP,
     * ENOENT}`.
     */
    static auto preAddValidate(const QFileInfo &file) noexcept -> pre_add_validate_t;

    /**
     * @brief Validates the provided absolute path of file `lnkPath`. Returns a `pre_rm_validate_t`
     * struct `{int status, QString lnkEntry, QString blobEntry}`.
     *
     * @param lnkPath: The path to link file for validation
     * @param enforcedCheck: (optional) Toggle stricter validation. Defaults to false.
     * @return pre_rm_validate_t: On validation passed, `status` is 0, `lnkEntry` contains the
     * constructed entry for `lnkPath` and `blobEntry` may either contain the name of the blob in
     * store or empty if `lnkPath` is resolved to be dangling. Orphaned blobs are never returned. On
     * error, `status` is `errno:{EAGAIN, ENOTDIR, EINVAL, ENOENT}`, `lnkEntry` may be empty but
     * `blobName` is always empty.
     */
    [[nodiscard]] auto preRmValidate(const QString &lnkPath,
                                     bool           enforcedCheck = false) const noexcept
            -> pre_rm_validate_t;

    /**
     * @brief Registers the entries to the in-memory hash-tables.
     *
     * @param lnkEntry: The symlink path pointing to `blobName`, not target path. Path name should
     * be relative to `dataDir`
     * @param blobName: The blob file name, usually the sha256 hash. Path name should be relative to
     * `dataDir/common`. Usually only the basename is accepted
     */
    auto registerEntry(const QString &lnkEntry, const QString &blobName) noexcept -> void;

    /**
     * @brief Releases the explicit thread lock & lock on the store.
     */
    auto releaseStoreLock() noexcept -> void;

    /**
     * @brief Removes the entries from the in-memory hash-tables.
     *
     * @param lnkEntry: The symlink path pointing to `blobName`, not target path. Path name should
     * be relative to `dataDir`
     * @param blobName: The blob file name, usually the sha256 hash. Path name should be relative to
     * `dataDir/common`. Usually only the basename is accepted
     */
    auto removeEntry(const QString &lnkEntry, const QString &blobName) noexcept -> void;

    /**
     * @brief Returns the lowest available rename suffixed name-string for `fileName` in `location`.
     * Empty if exceeds 1023 attempts.
     *
     * @param fileName: Base-name of the file
     * @param location: Directory where `fileName` should reside
     * @return QString: The rename suffixed name-string for `fileName`
     */
    static auto renameFile(const QString &fileName, const QDir &location) noexcept -> QString;

    /**
     * @brief Returns the absolute path to app's data dir as string; never returns null. Note this
     * function doesn't guarranty existence.
     *
     * @param orgName: The organization name, as would be set with
     * `QCoreApplication::setOrganizationName()`. Does not actually set the name application-wide.
     * Preferred over `QCoreApplication::organizationName()`. Fatal aborts if name contains invalid
     * path characters. Falls back to using `QCoreApplication::organizationName()` if empty.
     * @param appName: The application name, as would be set with
     * `QCoreApplication::setApplicationName()`. Does not actually set the name application-wide.
     * Preferred over `QCoreApplication::applicationName()`. Fatal aborts if name contains invalid
     * path characters. Falls back to using `QCoreApplication::applicationName()` if empty.
     * [IMPORTANT]: Fatal aborts if fallback resolutions of both `organizationName()` and
     * `applicationName()` resolves to empty.
     * @return QDir: The directory to app's computed data dir. Never null.
     */
    static auto resolveDataDir(const QString &orgName, const QString &appName) noexcept -> QDir;

    /**
     * @brief Overload of `resolveDataDir(const QString &orgName, const QString &appName)` taking
     * only the application name as parameter. The organization name is auto-inferred or if resolved
     * empty, only the application name is used.
     *
     * @param appName: The application name, as would be set with
     * `QCoreApplication::setApplicationName()`. Does not actually set the name application-wide.
     * Preferred over `QCoreApplication::applicationName()`. Fatal aborts if name contains invalid
     * path characters. Default is to infer the name from `QCoreApplication::applicationName()`.
     * [IMPORTANT]: Fatal aborts if fallback resolution of both `organizationName()` and
     * `applicationName()` resolves to empty.
     * @return QDir
     */
    static auto resolveDataDir(const QString &appName = { }) noexcept -> QDir;

    /**
     * @brief Function to reverse lookup a blob from symlinks in the category directories. Search
     * directories can be narrowed down further by specifying `categories`. Default is to search in
     * all category directories. Returns a `rev_lookup_symlink_t` struct having members {status,
     * locations}.
     *
     * @param blobName: Find the symlink matching this target file name
     * @param categories: Category directories to search the symlink in
     * @return rev_lookup_symlink_t: `locations` is a string-list of either paths of the symlinks
     * relative to storeDir or empty if none is found or faced other issues. `status` is `errno:{0,
     * EPERM}`.
     */
    auto revLookupSymlink(const QString &blobName, Categories categories = Category::All) noexcept
            -> rev_lookup_symlink_t;

    /**
     * @brief Sets read, write and execute permissions on a directory at path. Iterates from
     * strictest scope (owner level) to broadest (world level). Returns
     *
     * @param path: The directory path to set permissions
     * @param ec: Stores detailed errors about the failure.
     * @return bool: true on success, false if setting permission failed on all levels.
     */
    static auto setDirPerms(const std::filesystem::path &path, std::error_code &ec) noexcept
            -> bool;

    /**
     * @brief Shrinks the dictionary sizes to their contents size
     */
    auto squeezeDicts() noexcept -> void;

    /**
     * @brief Validates `blobFile` against expected attributes. `enforcedCheck` toggles pedantic and
     * rigorous checks. For most cases, this shouldn't be necessary.
     *
     * @param blobFile: The blob file to be validated. Recommended to construct with absolute file
     * path.
     * @param enforcedCheck: Toggles pedantic and performance costly validations.
     * @param categories: Strictly match the blob with these categories only. Requires pedantic
     * check to be on. Default matches with all valid categories.
     * @return int: 0 on validation pass, `errno:{EFBIG, EINVAL, ELOOP, EOPNOTSUPP, ENOENT}` on
     * failure.
     */
    static auto validateBlob(const QFileInfo &blobFile, bool enforcedCheck = false,
                             Categories categories = Category::All) noexcept -> int;

    /**
     * @brief File validation routines.
     *
     * @param file: File to validate
     * @return int: 0 if successful, errno:{EINVAL, ENOENT} otherwise.
     */
    static auto validateFile(const QFileInfo &file) noexcept -> int;

    /**
     * @brief Overload of `validateFile(const QFileInfo &)`. Additionally checks for file size
     * constraints.
     *
     * @param file: File to validate
     * @param fileSize: Maximum allowed file size
     * @return int: 0 if successful, errno:{EINVAL, ENOENT, EFBIG} otherwise.
     */
    static auto validateFile(const QFileInfo &file, qint64 fileSize) noexcept -> int;

    /**
     * @brief Validates the link file against expected attributes. Optionally takes a blob name to
     * additionally match against. Default is to validate attributes only.
     *
     * @param lnkFile: The link file to validate. Recommended to construct with absolute file path.
     * @param blobName: (optional) Blob name the symlink's target should match with.
     * @return int: 0 on validation passed, `errno:{ENOENT,EINVAL}` on failure
     */
    [[nodiscard]] auto validateLink(const QFileInfo &lnkFile, const QString &blobName = { }) const
            -> int;

    /**
     * @brief Validates and attempts repairs on dataDir, blobDir and categoryDir(s). Returns
     * validate_store_t struct having {int d_status, int h_status, int c_status}.
     *
     * @return validate_store_t: Values are 0 on success, errno on failure.
     */
    [[nodiscard]] auto validateStore() const noexcept -> validate_store_t;
};

class AddFileType : public QSharedData
{
public:
    int     status = 0;
    QUrl    location;
    QString id;
};

class RmFileType : public QSharedData
{
public:
    int status_l = 0;
    int status_b = 0;
};