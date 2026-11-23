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

// Static
auto FileManPrivate::calcDigest(const QString &path) noexcept -> QString
{
    QFile file{ path };

    if (!file.open(QIODeviceBase::ReadOnly))
        return { };

    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex();
}

auto FileManPrivate::clearAllEntries() noexcept -> void
{
    const QWriteLocker locker{ &this->dictLock };

    this->blobToLinkDict.clear();
    this->linkToBlobDict.clear();
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
        -> QHash<FileMan::Category, FileManPrivate::category_info_t>
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
        return ENOTDIR;
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

    logDebug << "blobToLinkDict:";
    for (const auto &[key, value] : std::as_const(this->blobToLinkDict).asKeyValueRange())
        logDebug << qPrintable(key) << ": " << value;

    logDebug << "linkToBlobDict:";
    for (const auto &[key, value] : std::as_const(this->linkToBlobDict).asKeyValueRange())
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
    case ENOTDIR:
        return 7;
    default:
        return 6;
    } // NOLINTEND(*magic-numbers*)
}

auto FileManPrivate::fixDirHealth(const QString &dir, int error,
                                  FileMan::Category category) const noexcept -> bool
{
    if (error != 0 && error != EAGAIN && error != ENOTDIR && error != EPERM) [[unlikely]] {
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

    if (error == ENOTDIR) [[unlikely]] {
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

auto FileManPrivate::lookupBlobByLink(const QString &lnkEntry) noexcept -> QString
{
    const QReadLocker locker{ &this->dictLock };
    return this->linkToBlobDict.value(lnkEntry, { });
}

auto FileManPrivate::lookupLinkByBlob(const QString &blobName) noexcept -> QString
{
    const QReadLocker locker{ &this->dictLock };
    return this->blobToLinkDict.value(blobName, { });
}

// Static
auto FileManPrivate::lowestSetFlag(const FileMan::Categories categories) noexcept -> Category
{
    const unsigned int v = categories.toInt();
    return (v == 0) ? Category::None : static_cast<Category>(v & (~v + 1U));
}

auto FileManPrivate::mkDataDir() const noexcept -> bool
{
    if (!QDir{ }.mkpath(this->blobDirPath)) [[unlikely]] {
        logCritical << "Failed to create" << this->blobDirPath;
        return false;
    }

    return true;
}

auto FileManPrivate::mkDataDir(FileMan::Category category) const noexcept -> bool
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
    const QDir      lnkDir      = lnkFile.dir();
    const QString   lnkFileName = lnkFile.fileName();
    const QString   lnkDirName  = lnkDir.dirName(); // i.e. parent dir name
    const QString   lnkDirPath  = QDir::cleanPath(lnkDir.absolutePath());
    const QString   lnkEntry    = FileManPrivate::createPathComp(lnkDirName, lnkFileName);

    // `lnkPath` could be a directory path or TLD, e.g. /, C: etc
    if (lnkFileName.isEmpty() || lnkDirName.isEmpty()) {
        logCritical << lnkPath << "is an invalid path. Refusing to proceed.";
        return { .status = EINVAL, .lnkEntry = { }, .blobEntry = { } };
    }

    const QString   categoryDirPath = this->categoryDirs.value(lnkDirName);
    const QDir      categoryDir     = categoryDirPath;
    const QFileInfo lnkTarget{ QDir::cleanPath(categoryDir.filePath(lnkFile.symLinkTarget())) };
    const QString   lnkTargetFileName = lnkTarget.fileName();

    if (categoryDirPath.isEmpty()) {
        logCritical << lnkPath << "is not a valid file in store";
        return { .status = EINVAL, .lnkEntry = { }, .blobEntry = { } };
    }

    if (lnkDirPath != categoryDirPath) {
        logCritical << lnkPath << "is not a path to store";
        return { .status = EINVAL, .lnkEntry = { }, .blobEntry = { } };
    }

    if (const int e = this->validateLink(lnkFile); e != 0) {
        if (e == EINVAL)
            return { .status = 0, .lnkEntry = lnkEntry, .blobEntry = { } };

        return { .status = e, .lnkEntry = { }, .blobEntry = { } };
    }

    //
    // Reaching here means provided symlink is valid and not dangling. Proceed to check the blobFile
    // next
    //

    return { .status    = 0,
             .lnkEntry  = lnkEntry,
             .blobEntry = (this->validateBlob(lnkTarget, enforcedCheck,
                                              FileMan::fromStringToCategory(lnkDirName))
                           == 0)
                     ? lnkTargetFileName
                     : QString{ } };
}

auto FileManPrivate::registerEntry(const QString &lnkEntry, const QString &blobName) noexcept
        -> void
{
    const QWriteLocker locker(&this->dictLock);

    this->blobToLinkDict.insert(blobName, lnkEntry);
    this->linkToBlobDict.insert(lnkEntry, blobName);
}

auto FileManPrivate::releaseStoreLock() noexcept -> void
{
    this->storeLock->unlock();
    this->storeMutex.unlock();
}

auto FileManPrivate::removeEntry(const QString &lnkEntry, const QString &blobName) noexcept -> void
{
    const QWriteLocker locker(&this->dictLock);

    this->blobToLinkDict.remove(blobName);
    this->linkToBlobDict.remove(lnkEntry);
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

auto FileManPrivate::revLookupSymlink(const QString      &blobName,
                                      FileMan::Categories categories) noexcept
        -> FileManPrivate::rev_lookup_symlink_t
{
#ifdef Q_OS_WIN
    // Enables permission lookup on NTFS on Windows within this scope
    const QNtfsPermissionCheckGuard permissionGuard;
#endif
    const QDir    blobDir     = this->blobDirPath;
    const QString blobPath    = blobDir.absoluteFilePath(blobName);
    const QString lnkEntry    = this->lookupLinkByBlob(blobName);
    const bool    entryExists = !lnkEntry.isEmpty();
    const bool    isManaged   = this->storeManaged;

    logDebug << "Looking up blob:" << blobName;

    if (isManaged && entryExists) [[likely]] {
        const QFileInfo lnkFile{ QDir{ this->storeDirPath }.filePath(lnkEntry) };
        // Implies existence _and_ symlink type (dangling or not)
        const bool      lnkExists =
                (lnkFile.isSymbolicLink() || lnkFile.isShortcut() || lnkFile.isAlias());

        if (lnkExists) [[likely]] {
            const QString lnkTarget =
                    QDir::cleanPath(blobDir.absoluteFilePath(lnkFile.symLinkTarget()));

            logDebug << "lnkTarget:" << lnkFile.symLinkTarget();
            logDebug << "lnkTarget (cleaned):" << lnkTarget;

            if (lnkTarget == blobPath) [[likely]]
                // The dictionaries only hold one map, not multi-maps
                return { .status = 0, .locations = { lnkEntry } };
        }

        // Either symlink doesn't match target in reality or it doesn't exist on fs. Either way,
        // remove stale entry from the dictionaries, and proceed to full dir check

        logDebug << "Removing stale" << lnkEntry << "from caches.";

        this->removeEntry(lnkEntry, blobName);
    }

    QStringList links;

    for (const auto &[category, catInfo] : this->categoryList.asKeyValueRange()) {

        if (!categories.testFlag(category))
            continue;

        logDebug << "Searching" << catInfo.name << "directory";

        const QDir             searchDir   = catInfo.location;
        // enlist oldest first
        const QList<QFileInfo> dirContents = searchDir.entryInfoList(
                QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot | QDir::System, QDir::Time);

        logDebug << "Directory contents:\n" << dirContents;

        if (dirContents.isEmpty()) {
            // Nothing present to lookup anyway
            logInfo << searchDir.absolutePath() << "is empty.";
            continue;
        }

        // Search for symlinks and compare the resolved target with `blobName`
        for (const QFileInfo &file : dirContents) {
            logDebug << "Examining file:" << file.filePath();

            if (!file.isSymbolicLink() && !file.isShortcut() && !file.isAlias()) [[unlikely]]
                continue;

            const QString resolvedTarget =
                    QDir::cleanPath(searchDir.absoluteFilePath(file.symLinkTarget()));

            logDebug << "Link target:"
                     << QDir::cleanPath(searchDir.absoluteFilePath(file.symLinkTarget()));

            if (resolvedTarget != blobPath)
                continue;

            const QString entry = FileManPrivate::createPathComp(catInfo.name, file.fileName());

            links.append(entry);

            if (isManaged) [[unlikely]] {
                logInfo << "Updating hash-tables with missing [" << entry << "," << blobName
                        << "] entry";

                this->registerEntry(entry, blobName);
            }
        }
    }

    if (!links.isEmpty())
        return { .status = 0, .locations = links };

    // Reaching here means no symlinks match `blobName`

    if (!isManaged && entryExists) [[unlikely]] // Stale entry in dictionaries
        this->removeEntry(lnkEntry, blobName);

    return { .status = ENOENT, .locations = { } };
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

    this->blobToLinkDict.squeeze();
    this->linkToBlobDict.squeeze();
}

auto FileManPrivate::validateBlob(const QFileInfo &blobFile, bool enforcedCheck,
                                  FileMan::Categories categories) const noexcept -> int
{
    const QString blobPath     = blobFile.absoluteFilePath();
    const QString blobFileName = blobFile.fileName();

    // Blob may be a crafted jump-symlink residing in the store
    if (blobFile.isSymbolicLink() || blobFile.isShortcut() || blobFile.isAlias()) [[unlikely]] {
        logWarning << blobPath << "is unexpectedly a symlink";
        return EINVAL;
    }

    // Blob may not pass our expected validations, `preAddValidate()` handles some of this
    // already, but do only when called for an enforced check since this is a costly function and
    // redos some jobs already done. Use the lighter `validateFile()` otherwise

    int e = 0;

    if (enforcedCheck) {
        const auto validateResult = this->preAddValidate(blobFile);

        if (!categories.testAnyFlags(validateResult.categories)) {
            logWarning << blobPath << "doesn't match required categories";
            return EINVAL;
        }

        e = validateResult.status;
    } else
        e = FileManPrivate::validateFile(blobFile);

    if (e == ENOENT) [[unlikely]] {
        logWarning << blobPath << "doesn't exist on store";
        return e;
    }

    if (e != 0) [[unlikely]] {
        logWarning << blobPath << "didn't pass validations";
        logDebug << "Error:" << e;
        return e;
    }

    // Valid files ought to have 64 characters exactly (sha256 digest length)
    constexpr qint8 nameLength = 64;

    if (const qint64 s = blobFileName.size(); s != nameLength) [[unlikely]]
    {
        logWarning << blobPath << "is not a valid file in store (wrong name)";
        logDebug << "Base name size:" << s << "vs" << nameLength;
        return EINVAL;
    }

    // They also have exactly hex characters: [a-fA-F] & [0-9]
    for (const QChar &c : blobFileName)
        if (!FileManPrivate::isHexDigit(c)) [[unlikely]] {
            logWarning << blobPath << "is not a valid file in store (wrong name)";
            logDebug << "Base name" << blobFileName << "doesn't look like a sha256 hash";
            return EINVAL;
        }

    // Validate the digest against file name. This is a costly operation so saving it for last and
    // call only when called for enforced check. Ducktyping above should already catch invalid files
    // 95-99% of the time.
    if (enforcedCheck)
        if (FileManPrivate::calcDigest(blobPath) != blobFileName) [[unlikely]] {
            logWarning << "Digest mismatch for" << blobPath;
            return EINVAL;
        }

    return 0;
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

auto FileManPrivate::validateLink(const QFileInfo &lnkFile, const QString &blobName) const -> int
{
    const QString lnkPath = lnkFile.absoluteFilePath();

    // Files under dataDir/<category> are usually symlinks
    if (!lnkFile.isSymbolicLink() && !lnkFile.isShortcut() && !lnkFile.isAlias()
        && !lnkFile.exists()) {
        // This file actually doesn't exist
        logWarning << lnkPath << "doesn't exist in store";
        return ENOENT;
    }

    const QString   lnkDirName    = lnkFile.dir().dirName();
    const QDir      categoryDir   = this->categoryDirs.value(lnkDirName, { });
    const QDir      blobDir       = this->blobDirPath;
    const QString   lnkTargetPath = QDir::cleanPath(categoryDir.filePath(lnkFile.symLinkTarget()));
    const QFileInfo lnkTarget{ lnkTargetPath };
    const QString   lnkTargetFileName = lnkTarget.fileName();

    logDebug << "Symlink target:" << lnkTargetPath;

    // All healthy symlinks canonically point to `blobDir`
    if (lnkTarget.absolutePath() != blobDir.absolutePath()) [[unlikely]] {
        // An alien symlink in the store, disallow symlink resolution
        logWarning << lnkPath << "points to outside the store or not a symbolic link";
        return EINVAL;
    }

    if (!blobName.isEmpty() && (lnkTargetFileName != blobName)) {
        logWarning << "Link target" << lnkTargetFileName
                   << "does not match with the provided file name" << blobName;
        return EINVAL;
    }

    return 0;
}

auto FileManPrivate::validateStore() const noexcept -> FileManPrivate::validate_store_t
{
    FileManPrivate::validate_store_t status = { .d_status = -1, .h_status = -1, .c_status = { } };

    for (const QString &dir : { this->storeDirPath, this->blobDirPath }) {

        const int e         = dirHealth(dir);
        int      &dirStatus = (dir == this->storeDirPath) ? status.d_status : status.h_status;

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