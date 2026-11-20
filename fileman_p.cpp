#include <QCoreApplication>
#include <QCryptographicHash>
#include <QMimeDatabase>
#include <QNtfsPermissionCheckGuard>
#include <QSet>
#include <QStandardPaths>
#include <QStorageInfo>

#include <fileman_p.hpp>

auto FileManPrivate::acquireStoreLock() noexcept -> int
{
    constexpr qint32 lockLifetime = 40000; // 40 sec
    constexpr qint32 waitDuration = 5000;  // 5 sec

    this->storeMutex.lock();
    this->storeLock->setStaleLockTime(lockLifetime);

    if (!this->storeLock->tryLock(waitDuration)) {
        int e = EIO;

        switch (this->storeLock->error()) {
        case QLockFile::LockFailedError:
            logCritical << "Store busy, could not acquire lock";
            e = EBUSY;
            break;
        case QLockFile::PermissionError:
            logCritical << "Permission error in store, could not acquire lock";
            e = EACCES;
            break;
        default:
            logCritical << "IO error in store, could not acquire lock";
            break;
        }

        this->storeMutex.unlock();

        return e;
    }

    return 0;
}

auto FileManPrivate::clearAllEntries() noexcept -> void
{
    const QWriteLocker locker{ &this->dictLock };

    this->hashToLinkDict.clear();
    this->linkToHashDict.clear();
}

// Static
auto FileManPrivate::compErrnoSeverity(int error1, int error2) noexcept -> int
{
    return (FileManPrivate::errnoSeverity(error1) > FileManPrivate::errnoSeverity(error2)) ? error1
                                                                                           : error2;
}

// Static
auto FileManPrivate::createCategoryDirsDict(const QString &dataDir) noexcept
        -> QHash<QString, QString>
{
    return {
        { "audio"_L1, FileManPrivate::createPathComp(dataDir, "audio"_L1) },
        { "image"_L1, FileManPrivate::createPathComp(dataDir, "image"_L1) },
        { "video"_L1, FileManPrivate::createPathComp(dataDir, "video"_L1) }
    };
}

// Static
auto FileManPrivate::createCategoryInfoDict(const QString &dataDir) noexcept
        -> QHash<Category, FileManPrivate::category_info_t>
{
    return {
        { Category::Audio,
         { .name = "audio"_L1, .location = FileManPrivate::createPathComp(dataDir, "audio"_L1) } },
        { Category::Image,
         { .name = "image"_L1, .location = FileManPrivate::createPathComp(dataDir, "image"_L1) } },
        { Category::Video,
         { .name = "video"_L1, .location = FileManPrivate::createPathComp(dataDir, "video"_L1) } }
    };
}

// Static
auto FileManPrivate::createPathComp(const QString &comp1, const QString &comp2) noexcept -> QString
{
    return QDir::toNativeSeparators(QDir::cleanPath(comp1 + '/'_L1 + comp2));
}

// Static
auto FileManPrivate::dirHealth(const QString &dirPath) noexcept -> int
{
    const QFileInfo dir{ dirPath };

    // Non-existence check; put first for better logging, otherwise, the following block should be
    // first
    if (!dir.exists()) {
        logWarning << dirPath << "doesn't exist yet";
        return EAGAIN;
    }

    // dirPath being a symlink is not allowed
    if (dir.isSymbolicLink() || dir.isShortcut() || dir.isAlias() || !dir.isDir()) [[unlikely]] {
        logCritical << dirPath << "is not a directory";
        return ECANCELED;
    }

    // Read, Write & Execute permissions are required
    if (!dir.isReadable() || !dir.isWritable() || !dir.isExecutable()) [[unlikely]] {
        logWarning << dirPath << "has inappropriate permissions";
        return EPERM;
    }

    return 0;
}

// Static
auto FileManPrivate::diskFreeCheck(const QFileInfo &file, const QDir &dir) noexcept -> int
{
    const qint64 fileSize = file.size();
    const qint64 fsFree   = QStorageInfo(dir).bytesAvailable();

    // `bytesAvailable()` needs to be guarded first.
    if (fsFree == -1) {
        logWarning << "Cannot retrieve file system free space, copy may fail";
        return -1;
    }

    if (fsFree - fileSize <= 0) {
        // Exact fit with 0 byte-margin is highly suspicious, and often prone to failure
        logCritical << "Not enough free space available";
        logInfo << fileSize << "required but only" << fsFree << "is available";
        return ENOSPC;
    }

    return 0;
}

auto FileManPrivate::dumpAllEntries() noexcept -> void
{
    const QReadLocker locker{ &this->dictLock };

    logDebug << "hashToLinkDict:";
    for (const auto &[key, value] : std::as_const(this->hashToLinkDict).asKeyValueRange())
        logDebug << qPrintable(key) << ": " << value;

    logDebug << "linkToHashDict:";
    for (const auto &[key, value] : std::as_const(this->linkToHashDict).asKeyValueRange())
        logDebug << qPrintable(key) << ": " << value;
}

// Static
auto FileManPrivate::errnoSeverity(int e) noexcept -> int
{
    switch (e) {
    case 0:
        return 0;
    case EAGAIN:
    case EBUSY:
        return 1;
    case ENOENT:
    case EEXIST:
        return 2;
    case EINVAL:
        return 3;
    case EACCES:
    case EPERM:
        return 4;
    case EIO: // NOLINTBEGIN(*magic-numbers*)
        return 5;
    case ECANCELED:
        return 7;
    default:
        return 6;
    } // NOLINTEND(*magic-numbers*)
}

auto FileManPrivate::fixDirHealth(const QString &dir, int error, Category category) const noexcept
        -> bool
{
    if (error != 0 && error != EAGAIN && error != ECANCELED && error != EPERM) [[unlikely]] {
        logCritical << "Critical error on store, can't continue";
        logDebug << "Error branch [" << dir << "]:" << error;
        return false;
    }

#ifdef Q_OS_WIN
    // Enables permission operations on NTFS on Windows within this scope
    QNtfsPermissionCheckGuard permissionGuard;
#endif

    if (error == EPERM) [[unlikely]] {
        // Attempt to fix permissions
        const std::filesystem::path dir_stl = dir.toStdString();
        std::error_code             ec;

        if (!FileManPrivate::setDirPerms(dir_stl, ec)) [[unlikely]] {
            logCritical << "Couldn't set appropriate permissions on" << dir;
            logDebug << "Error branch [" << dir << "]:" << error;
            logDebug << "Error code: " << ec.value();
            return false;
        }

        return true;
    }

    if (error == ECANCELED) [[unlikely]] {
        // QDir{dir}.remove() would require extracting parent dir and leaf dir names.
        // QFile::remove() is simpler and faster
        if (!QFile::remove(dir)) [[unlikely]] {

            logCritical << "Critical error on store, can't continue";
            logDebug << "Error branch [" << dir << "]:" << error;
            return false;
        }

        error = EAGAIN;
    }

    if (error == EAGAIN) [[likely]] {
        const bool dirCreated =
                (category == Category::None) ? this->mkDataDir() : this->mkDataDir(category);

        if (!dirCreated) [[unlikely]] {
            // Could be other advanced permission errors (e.g. ACL)
            logCritical << "Critical error on store, please check" << dir
                        << "and its parent dirs for permission issues";
            logDebug << "Error status [" << dir << "]:" << error;
            return false;
        }

        // Redundant, but future-proof
        return true;
    }

    return true;
}

// Static
auto FileManPrivate::hashFile(const QString &path) noexcept -> QString
{
    QFile file{ path };

    if (!file.open(QIODeviceBase::ReadOnly))
        return { };

    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex();
}

auto FileManPrivate::isDictEmpty(const QHash<QString, QString> &dict) noexcept -> bool
{
    const QReadLocker locker{ &this->dictLock };
    return dict.isEmpty();
}

// Static
auto FileManPrivate::isHexDigit(QChar ch) noexcept -> bool
{
    return (ch >= u'0' && ch <= u'9') || (ch >= u'a' && ch <= u'f') || (ch >= u'A' && ch <= u'F');
}

auto FileManPrivate::lookupHashByLink(const QString &lnkEntry) noexcept -> QString
{
    const QReadLocker locker{ &this->dictLock };
    return this->linkToHashDict.value(lnkEntry, { });
}

auto FileManPrivate::lookupLinkByHash(const QString &hash) noexcept -> QString
{
    const QReadLocker locker{ &this->dictLock };
    return this->hashToLinkDict.value(hash, { });
}

// Static
auto FileManPrivate::lowestSetFlag(const Categories categories) noexcept -> Category
{
    const unsigned int v = categories.toInt();
    return (v == 0) ? Category::None : static_cast<Category>(v & (~v + 1U));
}

auto FileManPrivate::mkDataDir() const noexcept -> bool
{
    if (!QDir{ }.mkpath(this->hashDir)) [[unlikely]] {
        logCritical << "Failed to create" << this->hashDir;
        return false;
    }

    return true;
}

auto FileManPrivate::mkDataDir(Category category) const noexcept -> bool
{
    const QString categoryDirPath{ this->categoryList.value(category).location };

    if (!QDir{ }.mkpath(categoryDirPath)) [[unlikely]] {
        logCritical << "Failed to create" << categoryDirPath;
        return false;
    }

    return true;
}

auto FileManPrivate::preAddValidate(const QFileInfo &file) const noexcept
        -> FileManPrivate::pre_add_validate_t
{
    if (file.isSymbolicLink() || file.isShortcut() || file.isAlias()) {
        //
        // A fuzzy function to test symlink recursion
        //
        // Someone could be joking or could actually be their OPSEC, but we shouldn't allow it too
        // far. Note, this is also handled by the filesystem natively but just in case
        //
        QString p            = QFileInfo{ file.symLinkTarget() }.absoluteFilePath();
        qint8   allowedLimit = 41; // NOLINT(*magic-numbers)

        while (allowedLimit-- > 0) {
            const QFileInfo t{ p };

            if (!t.isSymbolicLink() && !t.isShortcut() && !t.isAlias()) [[likely]]
                break;

            p = QFileInfo{ t.symLinkTarget() }.absoluteFilePath();
        }

        if (allowedLimit < 0) [[unlikely]]
            return { .status = ELOOP, .categories = Category::None };
    }

    // A 5 mins long 96 kHz 24-bit audio file could easily reach this ceiling images are much more
    // forgiving though
    constexpr qint32 gibVal = 1073741824;

    if (const int e = validateFile(file, gibVal); e != 0)
        return { .status = e, .categories = Category::None };

    //
    // Only take files having <category>/* mime types, e.g. audio/ogg, audio/flac, image/svg+xml,
    // image/png, video/mp4 etc this rules out text files common under unacceptable paths like /sys
    //
    const QMimeDatabase mimeDb;
    const QString       mime =
            mimeDb.mimeTypeForFile(file.canonicalFilePath(), QMimeDatabase::MatchContent)
                    .name()
                    .toLower();

    logDebug << "MIME:" << mime;

    if (mime.isEmpty()) [[unlikely]] {
        logCritical << "MIME type could not be determined";
        return { .status = EOPNOTSUPP, .categories = Category::None };
    }

    Categories categories = Category::None;

    for (const auto &[cat, catInfo] : this->categoryList.asKeyValueRange())
        if (mime.startsWith(catInfo.name + '/'_L1)
            || (catInfo.name == "video"_L1 && mime.contains("/x-matroska"_L1)))
            categories |= cat;

    if (categories == Category::None) {
        logCritical << "File type not supported for this operation";
        return { .status = EOPNOTSUPP, .categories = Category::None };
    }

    return { .status = 0, .categories = categories };
}

auto FileManPrivate::preRmValidate(const QString &lnkPath, const bool enforcedCheck) const noexcept
        -> FileManPrivate::pre_rm_validate_t
{
    const QFileInfo lnkFile{ lnkPath };
    const QString   lnkFileName = lnkFile.fileName();
    const QDir      lnkDir      = lnkFile.dir();
    const QString   lnkDirName  = lnkDir.dirName(); // i.e. parent dir name
    const QString   lnkDirPath  = QDir::cleanPath(lnkDir.absolutePath());
    const QString   lnkEntry    = FileManPrivate::createPathComp(lnkDirName, lnkFileName);

    // `lnkPath` could be a directory path or TLD /, C: etc
    if (lnkFileName.isEmpty() || lnkDirName.isEmpty()) {
        logCritical << lnkPath << "is an invalid path. Refusing to proceed.";
        return { .status = EINVAL, .lnkEntry = { }, .hashEntry = { } };
    }

    const QString categoryDirPath = this->categoryDirs.value(lnkDirName, { });
    const QDir    categoryDir{ categoryDirPath };

    if (categoryDirPath.isEmpty()) {
        logCritical << lnkPath << "is not a valid file in store";
        return { .status = EINVAL, .lnkEntry = { }, .hashEntry = { } };
    }

    if (lnkDirPath != categoryDirPath) {
        logCritical << lnkPath << "is not a path to store";
        return { .status = EINVAL, .lnkEntry = { }, .hashEntry = { } };
    }

    // Files under dataDir/<category> are usually symlinks
    if (!lnkFile.isSymbolicLink() && !lnkFile.isShortcut() && !lnkFile.isAlias()
        && !lnkFile.exists()) {
        // This file actually doesn't exist
        logWarning << lnkPath << "doesn't exist in store";
        return { .status = ENOENT, .lnkEntry = { }, .hashEntry = { } };
    }

    // See the target of the symlink first but don't resolve yet QDir::filePath() cleverly returns
    // the target path unchanged if it's absolute, lexically manipulates the string if it's relative
    const QDir      hashDir{ this->hashDir };
    const QString   lnkTargetPath = QDir::cleanPath(categoryDir.filePath(lnkFile.symLinkTarget()));
    const QFileInfo lnkTarget{ lnkTargetPath };
    const QString   lnkTargetFileName = lnkTarget.fileName();

    logDebug << "Symlink target:" << lnkTargetPath;

    // All healthy symlinks canonically point to `hashDir`
    if (lnkTarget.absolutePath() != hashDir.absolutePath()) [[unlikely]] {
        // An alien symlink in the store, disallow symlink resolution
        logWarning << lnkPath << "points to outside the store or not a symbolic link";
        return { .status = 0, .lnkEntry = lnkEntry, .hashEntry = { } };
    }

    // Target path may still be a crafted jump-symlink residing in the store
    if (lnkTarget.isSymbolicLink() || lnkTarget.isShortcut() || lnkTarget.isAlias()) [[unlikely]] {
        logWarning << lnkTargetPath << "is unexpectedly a symlink";
        return { .status = 0, .lnkEntry = lnkEntry, .hashEntry = { } };
    }

    // Target may not exist
    if (!lnkTarget.exists()) [[unlikely]] {
        logWarning << lnkTargetPath << "doesn't exist on store";
        return { .status = 0, .lnkEntry = lnkEntry, .hashEntry = { } };
    }

    //
    // Reaching here means provided symlink & target are both valid & safe and not dangling. Proceed
    // to check the hashFile next
    //

    // Target may not pass our expected validations, `preAddValidate()` handles some of this
    // already, but do only when called for an enforced check since this is a costly function and
    // redos some jobs already done. Use the lighter `validateFile()` otherwise

    const int e =
            (enforcedCheck) ? this->preAddValidate(lnkTarget).status : validateFile(lnkTarget);

    if (e != 0) [[unlikely]] {
        logWarning << lnkTargetPath << "didn't pass validations";
        logDebug << "Error:" << e;
        return { .status = 0, .lnkEntry = lnkEntry, .hashEntry = { } };
    }

    // Valid files ought to have 64 characters exactly (sha256 digest length)
    constexpr qint8 nameLength = 64;
    if (const qint64 s = lnkTargetFileName.size(); s != nameLength) [[unlikely]]
    {
        logWarning << lnkTargetPath << "is not a valid file in store (wrong name)";
        logDebug << "Base name size:" << s << "vs" << nameLength;
        return { .status = 0, .lnkEntry = lnkEntry, .hashEntry = { } };
    }

    // They also have exactly hex characters: [a-fA-F] & [0-9]
    for (const QChar &c : lnkTargetFileName)
        if (!FileManPrivate::isHexDigit(c)) [[unlikely]] {
            logWarning << lnkTargetPath << "is not a valid file in store (wrong name)";
            logDebug << "Base name" << lnkTargetFileName << "doesn't look like a sha256 hash";
            return { .status = 0, .lnkEntry = lnkEntry, .hashEntry = { } };
        }

    // Validate the digest against file name. This is a costly operation so saving this for last and
    // only when called for enforced check
    if (enforcedCheck)
        if (FileManPrivate::hashFile(lnkTargetPath) != lnkTargetFileName) [[unlikely]] {
            logWarning << "Digest mismatch for" << lnkTargetPath;
            return { .status = 0, .lnkEntry = lnkEntry, .hashEntry = { } };
        }

    return { .status = 0, .lnkEntry = lnkEntry, .hashEntry = lnkTargetFileName };
}

auto FileManPrivate::registerEntry(const QString &lnkEntry, const QString &hash) noexcept -> void
{
    const QWriteLocker locker(&this->dictLock);

    this->hashToLinkDict.insert(hash, lnkEntry);
    this->linkToHashDict.insert(lnkEntry, hash);
}

auto FileManPrivate::releaseStoreLock() noexcept -> void
{
    this->storeLock->unlock();
    this->storeMutex.unlock();
}

auto FileManPrivate::removeEntry(const QString &lnkEntry, const QString &hash) noexcept -> void
{
    const QWriteLocker locker(&this->dictLock);

    this->hashToLinkDict.remove(hash);
    this->linkToHashDict.remove(lnkEntry);
}

// Static
auto FileManPrivate::renameFile(const QString &fileName, const QDir &location) noexcept -> QString
{
    // Trivial case: fileName is already unique, no renames necessary
    if (location.exists(fileName)) [[likely]]
        return fileName;

    // Find the first empty rename suffix; e.g. existing-name_2.ext having even anything near 1000
    // rename-suffixed same files is atypical
    const QFileInfo  file{ fileName };
    const QString    baseName = file.completeBaseName();
    const QString    suffix   = u"."_s + file.suffix();
    constexpr qint64 lim      = 1024;

    // Using fast & efficient O(1) QSet for lookup rather than expending IO QDir::exists()
    const QStringList   contents = location.entryList();
    const QSet<QString> contentsSet(contents.begin(), contents.end());

    for (int i = 1; i < lim; ++i) {
        const QString candid = baseName + u"_"_s + QString::number(i) + suffix;

        if (!contentsSet.contains(candid))
            return candid;
    }

    logWarning << "File count limit exceeded";
    return { };
}

// Static
auto FileManPrivate::resolveDataDir(const QString &orgName, const QString &appName) noexcept -> QDir
{
    QDir appDataDir;

    if (orgName.isEmpty() && appName.isEmpty()) {
        appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

        // Edge-case when both `applicationName()` and `organizationName()` may not be set
        if (appDataDir == QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
                [[unlikely]]
            logFatal << "Neither application or organization name are set. FileMan cannot safely "
                        "isolate the store. Aborting.";

        return appDataDir;
    }

    if (QFileInfo(appName).fileName() != appName)
        logFatal << "Invalid application name. FileMan cannot safely isolate its data store. "
                    "Aborting";

    if (QCoreApplication::applicationName() != appName)
        logWarning << "QCoreApplication::applicationName() and provided application names differ. "
                      "FileMan will use the provided name. Make sure this is intended.";

    QString org;

    if (orgName.isEmpty())
        org = QCoreApplication::organizationName();
    else {
        if (QFileInfo(orgName).fileName() != orgName)
            logFatal << "Invalid organization name. FileMan cannot safely isolate its data store. "
                        "Aborting";

        if (QCoreApplication::organizationName() != orgName)
            logWarning << "QCoreApplication::organizationName() and provided organization names "
                          "differ. FileMan will use the provided name. Make sure this is intended.";

        org = orgName;
    }

    appDataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString relativePath = (org.isEmpty()) ? appName : org + u'/' + appName;

    logDebug << "Store dir:" << appDataDir.absoluteFilePath(relativePath);

    return appDataDir.filePath(relativePath);
}

// Static
auto FileManPrivate::resolveDataDir(const QString &appName) noexcept -> QDir
{
    return FileManPrivate::resolveDataDir({ }, appName);
}

auto FileManPrivate::revLookupSymlink(const QDir &searchDir, const QString &targetPath) noexcept
        -> FileManPrivate::rev_lookup_symlink_t
{
#ifdef Q_OS_WIN
    // Enables permission lookup on NTFS on Windows within this scope
    QNtfsPermissionCheckGuard permissionGuard;
#endif

    // Just for logging purposes, should show the original format it was constructed with
    const QString searchDirPath = searchDir.path();

    // Searching files and looking up symlink targets only need the executable permission
    if (!QFileInfo{ searchDir, "."_L1 }.isExecutable())
        return { .status = EPERM, .location = { } };

    // Let's assume `targetPath` is hashDir/hash and try some shortcuts
    const QString targetName  = QFileInfo{ targetPath }.fileName();
    const QString lnkEntry    = this->lookupLinkByHash(targetName);
    const bool    entryExists = !lnkEntry.isEmpty();
    const bool    isManaged   = this->storeManaged;

    if (isManaged && entryExists) [[likely]] {
        const QFileInfo lnkFile{ QDir{ this->dataDir }.filePath(lnkEntry) };
        // Implies existence _and_ symlink type (dangling or not)
        const bool      lnkExists =
                (lnkFile.isSymbolicLink() || lnkFile.isShortcut() || lnkFile.isAlias());

        logDebug << "targetPath:" << targetPath;

        if (lnkExists) [[likely]] {
            const QString lnkTarget =
                    QDir::cleanPath(searchDir.absoluteFilePath(lnkFile.symLinkTarget()));

            logDebug << "searchDir:" << searchDirPath;
            logDebug << "lnkTarget:" << lnkFile.symLinkTarget();
            logDebug << "lnkTarget (cleaned):" << lnkTarget;

            if (lnkTarget == QDir::cleanPath(targetPath))
                return { .status = 0, .location = QDir::cleanPath(lnkFile.absoluteFilePath()) };
        }

        // Either symlink doesn't match target in reality or it doesn't exist on fs. Either way,
        // remove stale entry from the dictionaries, and proceed to full dir check

        logDebug << "Removing stale" << lnkEntry << "from caches.";

        this->removeEntry(lnkEntry, targetName);
    }

    const QList<QFileInfo> dirContents = searchDir.entryInfoList(
            QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot | QDir::System);

    if (dirContents.isEmpty()) {
        // Nothing present to lookup anyway
        logDebug << searchDirPath << "is empty.";
        return { .status = EAGAIN, .location = { } };
    }

    // Search for symlinks and compare the resolved target with `target`
    for (const QFileInfo &file : dirContents) {

        if (!file.isSymbolicLink() && !file.isShortcut() && !file.isAlias()) [[unlikely]]
            continue;

        const QString resolvedTarget =
                QDir::cleanPath(searchDir.absoluteFilePath(file.symLinkTarget()));

        if (resolvedTarget == QDir::cleanPath(targetPath)) [[likely]] {

            if (isManaged) [[unlikely]] {
                const QString lnkEntry =
                        FileManPrivate::createPathComp(file.dir().dirName(), file.fileName());

                logDebug << "Updating hash-tables with missing [" << lnkEntry << "," << targetName
                         << "] entry";

                this->registerEntry(lnkEntry, targetName);
            }

            return { .status = 0, .location = QDir::cleanPath(file.absoluteFilePath()) };
        }
    }

    // Reaching here means no symlinks match `target`

    if (!isManaged && entryExists) [[unlikely]] // Stale entry in dictionaries
        this->removeEntry(lnkEntry, targetName);

    return { .status = ENOENT, .location = { } };
}

// Static
auto FileManPrivate::setDirPerms(const std::filesystem::path &dirPath, std::error_code &ec) noexcept
        -> bool
{
    using std::filesystem::perm_options;
    using std::filesystem::permissions;
    using std::filesystem::perms;

    const QList<perms> permScope = { perms::owner_all, perms::group_all, perms::others_all };

    for (const perms &scope : permScope) {
        permissions(dirPath, scope, perm_options::add, ec);

        if (ec.value() == 0) [[likely]]
            return true;
    }

    return false;
}

auto FileManPrivate::squeezeDicts() noexcept -> void
{
    QWriteLocker locker(&this->dictLock);

    this->hashToLinkDict.squeeze();
    this->linkToHashDict.squeeze();
}

// Static
auto FileManPrivate::validateFile(const QFileInfo &file) noexcept -> int
{
    //
    // Check existence & file type; only take regular files - from wherever accessible (including
    // from /sys as long as permission allows but we handle these unacceptable files next)
    //
    if (const bool exists = file.exists(); !exists || !file.isFile())
        return exists ? EINVAL : ENOENT;

    // regular, but unusual files; e.g. textstreams or files under /proc
    if (file.size() <= 0)
        return EINVAL;

    return 0;
}

// Static
auto FileManPrivate::validateFile(const QFileInfo &file, const qint64 fileSize) noexcept -> int
{
    if (const int e = validateFile(file); e != 0)
        return e;

    if (file.size() > fileSize) {
        logCritical << "File size exceeding" << QLocale{ }.formattedDataSize(fileSize)
                    << "is not supported";
        return EFBIG;
    }

    return 0;
}

auto FileManPrivate::validateStore() const noexcept -> FileManPrivate::validate_store_t
{
    FileManPrivate::validate_store_t status = { .d_status = -1, .h_status = -1, .c_status = { } };

    for (const QString &dir : { this->dataDir, this->hashDir }) {

        const int e         = dirHealth(dir);
        int      &dirStatus = (dir == this->dataDir) ? status.d_status : status.h_status;

        logDebug << dir << "directory health:" << e;

        if (!FileManPrivate::fixDirHealth(dir, e)) {
            dirStatus = e;

            // The checks are serialized based on hierarchy so any errors are fatal, blocking
            // further continuation

            // logDebug << "Error status [d, h, c]:" << status.d_status << status.h_status
            //          << status.c_status;

            return status;
        }

        dirStatus = 0;
    }

    for (const auto &[cat, catInfo] : this->categoryList.asKeyValueRange()) {

        logDebug << "Store path (categorized):" << catInfo.location;

        const int e = FileManPrivate::dirHealth(catInfo.location);

        logDebug << catInfo.name << "directory health:" << e;

        if (!FileManPrivate::fixDirHealth(catInfo.location, e, cat)) {
            status.c_status.insert(cat, e);
            continue;
        }

        status.c_status.insert(cat, 0);
    }

    // logDebug << "Error status [d, h, c]:" << status.d_status << status.h_status <<
    // status.c_status;

    return status;
}