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

    this->m_storeMutex.lock();
    this->p_storeLock->setStaleLockTime(lockLifetime);

    if (!this->p_storeLock->tryLock(waitDuration)) {
        int e = EIO;

        switch (this->p_storeLock->error()) {
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

        this->m_storeMutex.unlock();

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
    const QWriteLocker lock = &this->m_varLock;

    this->m_blobToLinkDict.clear();
    this->m_linkToBlobDict.clear();
}

// Static
auto FileManPrivate::compErrnoSeverity(int error1, int error2) noexcept -> int
{
    return (FileManPrivate::errnoSeverity(error1) > FileManPrivate::errnoSeverity(error2)) ? error1
                                                                                           : error2;
}

// Static
auto FileManPrivate::createPathComp(const QString &comp1, const QString &comp2) noexcept -> QString
{
    return QDir::toNativeSeparators(QDir::cleanPath(comp1 + u'/' + comp2));
}

// Static
auto FileManPrivate::dirHealth(const QString &dirPath) noexcept -> int
{
    const QFileInfo dir{ dirPath };

    // Non-existence check; put first for better logging, otherwise, the following block should be
    // first
    if (!dir.exists()) {
        logWarning << dirPath << " doesn't exist yet";
        return EAGAIN;
    }

    // dirPath being a symlink is not allowed
    if (dir.isSymbolicLink() || dir.isShortcut() || dir.isAlias() || !dir.isDir()) [[unlikely]] {
        logCritical << dirPath << " is not a directory";
        return ENOTDIR;
    }

#ifdef Q_OS_WIN
    const QNtfsPermissionCheckGuard permissionGuard;
#endif

    // Read, Write & Execute permissions are required
    if (!dir.isReadable() || !dir.isWritable() || !dir.isExecutable()) [[unlikely]] {
        logWarning << dirPath << " has inappropriate permissions";
        return EACCES;
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
        logInfo << fileSize << " required but only " << fsFree << " is available";
        return ENOSPC;
    }

    return 0;
}

auto FileManPrivate::dumpAllEntries() noexcept -> void
{
    const QReadLocker lock = &this->m_varLock;

    logDebug << "blobToLinkDict:";
    for (const auto &[key, value] : std::as_const(this->m_blobToLinkDict).asKeyValueRange())
        logDebug << qPrintable(key) << ": " << value;

    logDebug << "linkToBlobDict:";
    for (const auto &[key, value] : std::as_const(this->m_linkToBlobDict).asKeyValueRange())
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
        // case EPERM:
        return 4;
    case EIO: // NOLINTBEGIN(*magic-numbers*)
        return 5;
    case ECANCELED | ENOTDIR:
        return 7;
    default:
        return 6;
    } // NOLINTEND(*magic-numbers*)
}

auto FileManPrivate::fixDirHealth(const QString &dir, int error,
                                  FileMan::Category category) const noexcept -> bool
{
    if (error != 0 && error != EAGAIN && error != ENOTDIR && error != EACCES) [[unlikely]] {
        logCritical << "Critical error on store, can't continue";
        logDebug << "Error branch [" << dir << "]: " << error;
        return false;
    }

    if (error == EACCES) [[unlikely]] {
        // Attempt to fix permissions
        if (!QFile::setPermissions(dir, FileManPrivate::sc_dirPerms)) [[unlikely]] {
            logCritical << "Couldn't set appropriate permissions on " << dir;
            return false;
        }

        return true;
    }

    if (error == ENOTDIR) [[unlikely]] {
        // QDir{dir}.remove() would require extracting parent dir and leaf dir names.
        // QFile::remove() is simpler and faster
        if (!QFile::remove(dir)) [[unlikely]] {

            logCritical << "Critical error on store, can't continue";
            logDebug << "Error branch [" << dir << "]: " << error;
            return false;
        }

        error = EAGAIN;
    }

    if (error == EAGAIN) [[likely]] {
        const bool dirCreated =
                (category == Category::None) ? this->mkDataDir() : this->mkDataDir(category);

        if (!dirCreated) [[unlikely]] {
            // Could be other advanced permission errors (e.g. ACL)
            logCritical << "Critical error on store, please check " << dir
                        << " and its parent dirs for permission issues";
            logDebug << "Error status [" << dir << "]: " << error;
            return false;
        }

        // Redundant, but future-proof
        return true;
    }

    return true;
}

// Static
auto FileManPrivate::initPathsAndDicts(const QDir &storeDir) noexcept -> void
{
    this->m_storeDirPath = storeDir.absoluteFilePath(FileManPrivate::sc_storeDirName);

    const QDir baseDir = this->m_storeDirPath;

    this->m_blobDirPath = baseDir.absoluteFilePath(FileManPrivate::sc_blobDirName);

    const QString audiosPath =
            baseDir.absoluteFilePath(FileManPrivate::sc_categoryToDirNames.value(Category::Audio));
    const QString imagesPath =
            baseDir.absoluteFilePath(FileManPrivate::sc_categoryToDirNames.value(Category::Image));
    const QString videosPath =
            baseDir.absoluteFilePath(FileManPrivate::sc_categoryToDirNames.value(Category::Video));

    this->m_categoryToDirs = {
        { Category::Audio, audiosPath },
        { Category::Image, imagesPath },
        { Category::Video, videosPath },
    };

    this->m_dirsToCategory = {
        { audiosPath, Category::Audio },
        { imagesPath, Category::Image },
        { videosPath, Category::Video },
    };
}

auto FileManPrivate::isDictEmpty(const QHash<QString, QString> &dict) noexcept -> bool
{
    const QReadLocker lock = &this->m_varLock;
    return dict.isEmpty();
}

// Static
auto FileManPrivate::isHexDigit(QChar ch) noexcept -> bool
{
    return (ch >= u'0' && ch <= u'9') || (ch >= u'a' && ch <= u'f') || (ch >= u'A' && ch <= u'F');
}

// Static
auto FileManPrivate::isNtfs(const QDir &dir) noexcept -> bool
{
    const QString fsType = QStorageInfo{ dir }.fileSystemType();
    return fsType.contains("ntfs"_L1, Qt::CaseInsensitive)
            || fsType.contains("fuse"_L1, Qt::CaseInsensitive);
}

auto FileManPrivate::lookupBlobByLink(const QString &lnkEntry) noexcept -> QString
{
    const QReadLocker lock = &this->m_varLock;
    return this->m_linkToBlobDict.value(lnkEntry);
}

auto FileManPrivate::lookupLinkByBlob(const QString &blobName) noexcept -> QString
{
    const QReadLocker lock = &this->m_varLock;
    return this->m_blobToLinkDict.value(blobName);
}

// Static
auto FileManPrivate::lowestSetFlag(const FileMan::Categories categories) noexcept -> Category
{
    const unsigned int v = categories.toInt();
    return (v == 0) ? Category::None : static_cast<Category>(v & (~v + 1U));
}

auto FileManPrivate::mkDataDir() const noexcept -> bool
{
    return QDir{ }.mkpath(this->m_blobDirPath);
}

auto FileManPrivate::mkDataDir(FileMan::Category category) const noexcept -> bool
{
    return QDir{ }.mkpath(this->m_categoryToDirs.value(category));
}

// Static
auto FileManPrivate::preAddValidate(const QFileInfo &file) noexcept
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
    static constexpr qint32 gibVal = 1073741824;

    if (const int e = FileManPrivate::validateFile(file, gibVal); e != 0)
        return { .status = e, .categories = Category::None };

    //
    // Only take files having <category>/* mime types, e.g. audio/ogg, audio/flac, image/svg+xml,
    // image/png, video/mp4 etc this rules out text files common under unacceptable paths like /sys
    //
    const QMimeDatabase mimeDb;
    const QString       mime =
            mimeDb.mimeTypeForFile(file.canonicalFilePath(), QMimeDatabase::MatchContent).name();

    logDebug << "MIME: " << mime;

    if (mime.isEmpty()) [[unlikely]] {
        logCritical << "MIME type could not be determined";
        return { .status = EOPNOTSUPP, .categories = Category::None };
    }

    Categories categories = Category::None;

    for (const auto &[cat, name] : FileManPrivate::sc_categoryToString.asKeyValueRange())
        if (mime.startsWith(name + u'/', Qt::CaseInsensitive)
            || (name.compare("video"_L1, Qt::CaseInsensitive) == 0
                && mime.contains("/x-matroska"_L1, Qt::CaseInsensitive)))
            categories |= cat;

    if (categories == Category::None) {
        logCritical << "File type not supported for this operation";
        return { .status = ECANCELED, .categories = Category::None };
    }

    return { .status = 0, .categories = categories };
}

auto FileManPrivate::preRmValidate(const QString &lnkPath, const bool enforcedCheck) const noexcept
        -> FileManPrivate::pre_rm_validate_t
{
    const QFileInfo lnkFile{ lnkPath };
    const QString   lnkDirPath = QDir::cleanPath(lnkFile.absolutePath());
    const QString   lnkEntry =
            FileManPrivate::createPathComp(lnkFile.dir().dirName(), lnkFile.fileName());
    const Category category = this->m_dirsToCategory.value(lnkDirPath, Category::None);

    if (const int e = this->validateLink(lnkFile); e != 0)
        return { .status    = (e == EINVAL) ? 0 : e,
                 .lnkEntry  = (e == EINVAL) ? lnkEntry : QString{ },
                 .blobEntry = { } };

    //
    // Reaching here means provided symlink is valid and not dangling. Proceed to check the target
    // next
    //

    const QFileInfo lnkTarget{ QDir::cleanPath(
            QDir{ lnkDirPath }.filePath(lnkFile.symLinkTarget())) };
    const QString   lnkTargetFileName = lnkTarget.fileName();

    return { .status    = 0,
             .lnkEntry  = lnkEntry,
             .blobEntry = (FileManPrivate::validateBlob(lnkTarget, enforcedCheck, category) == 0)
                     ? lnkTargetFileName
                     : QString{ } };
}

auto FileManPrivate::registerEntry(const QString &lnkEntry, const QString &blobName) noexcept
        -> void
{
    const QWriteLocker lock = &this->m_varLock;

    this->m_blobToLinkDict.insert(blobName, lnkEntry);
    this->m_linkToBlobDict.insert(lnkEntry, blobName);
}

auto FileManPrivate::releaseStoreLock() noexcept -> void
{
    this->p_storeLock->unlock();
    this->m_storeMutex.unlock();
}

auto FileManPrivate::removeEntry(const QString &lnkEntry, const QString &blobName) noexcept -> void
{
    const QWriteLocker lock = &this->m_varLock;

    this->m_blobToLinkDict.remove(blobName);
    this->m_linkToBlobDict.remove(lnkEntry);
}

// Static
auto FileManPrivate::renameFile(const QString &fileName, const QDir &location) noexcept -> QString
{
    // Trivial case: fileName is already unique, no renames necessary
    if (location.exists(fileName)) [[likely]]
        return fileName;

    // Find the first empty rename suffix; e.g. existing-name_2.ext. Having even anything near 1000
    // rename-suffixed same files is atypical
    const QFileInfo         file{ fileName };
    const QString           baseName = file.completeBaseName();
    const QString           suffix   = u'.' + file.suffix();
    static constexpr qint64 lim      = 1024;

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

    if (QFileInfo{ appName }.fileName() != appName)
        logFatal << "Invalid application name. FileMan cannot safely isolate its data store. "
                    "Aborting";

    if (QCoreApplication::applicationName() != appName)
        logWarning << "QCoreApplication::applicationName() and provided application names differ. "
                      "FileMan will use the provided name. Make sure this is intended.";

    QString org;

    if (orgName.isEmpty())
        org = QCoreApplication::organizationName();
    else {
        if (QFileInfo{ orgName }.fileName() != orgName)
            logFatal << "Invalid organization name. FileMan cannot safely isolate its data store. "
                        "Aborting";

        if (QCoreApplication::organizationName() != orgName)
            logWarning << "QCoreApplication::organizationName() and provided organization names "
                          "differ. FileMan will use the provided name. Make sure this is intended.";

        org = orgName;
    }

    appDataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);

    const QString relativePath = (org.isEmpty()) ? appName : org + u'/' + appName;

    logDebug << "Store dir: " << appDataDir.absoluteFilePath(relativePath);

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
    const QDir    blobDir     = this->m_blobDirPath;
    const QString blobPath    = blobDir.absoluteFilePath(blobName);
    const QString lnkEntry    = this->lookupLinkByBlob(blobName);
    const bool    entryExists = !lnkEntry.isEmpty();
    const bool    isManaged   = this->m_storeManaged;

    logDebug << "Looking up blob: " << blobName;

    if (isManaged && entryExists) [[likely]] {
        const QFileInfo lnkFile{ QDir{ this->m_storeDirPath }.filePath(lnkEntry) };
        // Implies existence _and_ symlink type (dangling or not)
        const bool      lnkExists =
                (lnkFile.isSymbolicLink() || lnkFile.isShortcut() || lnkFile.isAlias());

        if (lnkExists) [[likely]] {
            const QString lnkTarget =
                    QDir::cleanPath(blobDir.absoluteFilePath(lnkFile.symLinkTarget()));

            logDebug << "lnkTarget: " << lnkFile.symLinkTarget();
            logDebug << "lnkTarget (cleaned): " << lnkTarget;

            if (lnkTarget == blobPath) [[likely]]
                // The dictionaries only hold one map, not multi-maps
                return { .status = 0, .locations = { lnkEntry } };
        }

        // Either symlink doesn't match target in reality or it doesn't exist on fs. Either way,
        // remove stale entry from the dictionaries, and proceed to full dir check

        logDebug << "Removing stale " << lnkEntry << " from caches.";

        this->removeEntry(lnkEntry, blobName);
    }

    QStringList links;

    for (const auto &[cat, catDirPath] : this->m_categoryToDirs.asKeyValueRange()) {

        if (!categories.testFlag(cat))
            continue;

        logDebug << "Searching " << catDirPath << " directory";

        const QDir             searchDir   = catDirPath;
        // enlist oldest first
        const QList<QFileInfo> dirContents = searchDir.entryInfoList(
                QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot | QDir::System, QDir::Time);

        logDebug << "Directory contents:\n" << dirContents;

        if (dirContents.isEmpty()) {
            // Nothing present to lookup anyway
            logInfo << searchDir.absolutePath() << " is empty.";
            continue;
        }

        // Search for symlinks and compare the resolved target with `blobName`
        for (const QFileInfo &file : dirContents) {
            logDebug << "Examining file: " << file.filePath();

            if (!file.isSymbolicLink() && !file.isShortcut() && !file.isAlias()) [[unlikely]]
                continue;

            const QString resolvedTarget =
                    QDir::cleanPath(searchDir.absoluteFilePath(file.symLinkTarget()));

            logDebug << "Link target: " << resolvedTarget;

            if (resolvedTarget != blobPath)
                continue;

            const QString entry = FileManPrivate::createPathComp(
                    FileManPrivate::sc_categoryToDirNames.value(cat), file.fileName());

            links.append(entry);

            if (isManaged) [[unlikely]] {
                logInfo << "Updating hash-tables with missing [" << entry << ", " << blobName
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

auto FileManPrivate::squeezeDicts() noexcept -> void
{
    const QWriteLocker lock = &this->m_varLock;

    this->m_blobToLinkDict.squeeze();
    this->m_linkToBlobDict.squeeze();
}

// Static
auto FileManPrivate::validateBlob(const QFileInfo &blobFile, bool enforcedCheck,
                                  FileMan::Categories categories) noexcept -> int
{
    const QString blobPath     = blobFile.absoluteFilePath();
    const QString blobFileName = blobFile.fileName();

    // Blob may be a crafted jump-symlink residing in the store
    if (blobFile.isSymbolicLink() || blobFile.isShortcut() || blobFile.isAlias()) [[unlikely]] {
        logWarning << blobPath << " is unexpectedly a symlink";
        return EINVAL;
    }

    // Blob may not pass our expected validations, `preAddValidate()` handles some of this
    // already, but do only when called for an enforced check since this is a costly function and
    // redos some jobs already done. Use the lighter `validateFile()` otherwise

    int        e = 0;
    Categories fileCategories;
    QFileInfo  tmpBlobFile = blobFile;

    for (bool retry = true; retry;) {

        if (enforcedCheck) {
            const auto validateResult = FileManPrivate::preAddValidate(tmpBlobFile);
            e                         = validateResult.status;
            fileCategories            = validateResult.categories;
        } else
            e = FileManPrivate::validateFile(blobFile);

        if (e == ENOENT) [[unlikely]] {
            logWarning << blobPath << " doesn't exist on store";
            return e;
        }

        // Attempt to fix the permission if file is not readable
        if (e == EACCES) [[unlikely]] {
            logWarning << blobPath << " has inappropriate permissions. Attempting fix...";

            if (!QFile::setPermissions(blobPath, FileManPrivate::sc_filePerms)) [[unlikely]] {
                logCritical << "Couldn't set appropriate permissions on " << blobPath;
                return e;
            }

            // Check again after permission fix. Also requires a QFileInfo::refresh() to invalidate
            // the cache
            tmpBlobFile.refresh();
            continue;
        }

        retry = false;
    }

    if (e != 0) [[unlikely]] {
        logWarning << blobPath << " didn't pass validations";
        logDebug << "Error: " << e;
        return e;
    }

    if (enforcedCheck && !categories.testAnyFlags(fileCategories)) {
        logWarning << blobPath << " doesn't match required categories";
        return EINVAL;
    }

    // Valid files ought to have 64 characters exactly (sha256 digest length)
    static constexpr qsizetype nameLength = 64;

    if (const qint64 s = blobFileName.size(); s != nameLength) [[unlikely]]
    {
        logWarning << blobPath << " is not a valid file in store (wrong name)";
        logDebug << "Base name size: " << s << " vs " << nameLength;
        return EINVAL;
    }

    // They also have exactly hex characters: [a-fA-F] & [0-9]
    for (const QChar &c : blobFileName)
        if (!FileManPrivate::isHexDigit(c)) [[unlikely]] {
            logWarning << blobPath << " is not a valid file in store (wrong name)";
            logDebug << "Base name " << blobFileName << " doesn't look like a sha256 hash";
            return EINVAL;
        }

    // Validate the digest against file name. This is a costly operation so saving it for last and
    // call only when called for enforced check. Ducktyping above should already catch invalid files
    // 95-99% of the time.
    if (enforcedCheck && FileManPrivate::calcDigest(blobPath) != blobFileName) [[unlikely]] {
        logWarning << "Digest mismatch for " << blobPath;
        return EINVAL;
    }

    return e;
}

// Static
auto FileManPrivate::validateFile(const QFileInfo &file) noexcept -> int
{
#ifdef Q_OS_WIN
    const QNtfsPermissionCheckGuard permissionGuard;
#endif

    // Check existence & file type; only take regular files - from wherever accessible (including
    // from /sys as long as permission allows but we handle these unacceptable files next)
    if (const bool exists = file.exists(); !exists || !file.isFile())
        return exists ? EINVAL : ENOENT;

    // Valid files at least need read permission
    if (!file.isReadable()) [[unlikely]]
        return EACCES;

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
        logCritical << "File size exceeding " << QLocale{ }.formattedDataSize(fileSize)
                    << " is not supported";
        return EFBIG;
    }

    return 0;
}

auto FileManPrivate::validateLink(const QFileInfo &lnkFile, const QString &blobName) const -> int
{
    const QString  lnkPath    = lnkFile.absoluteFilePath();
    const QString  lnkDirPath = lnkFile.absolutePath();
    const Category category   = this->m_dirsToCategory.value(lnkDirPath, Category::None);

    // `lnkFile` could be a directory path or TLD, e.g. /, C: etc
    if (lnkFile.fileName().isEmpty() || lnkFile.dir().dirName().isEmpty()) {
        logCritical << lnkPath << " is an invalid path. Refusing to proceed.";
        return ECANCELED;
    }

    if (category == Category::None) {
        logCritical << lnkPath << " is not a path to store";
        return ECANCELED;
    }

    // Files under dataDir/<category> are usually symlinks
    if (!lnkFile.isSymbolicLink() && !lnkFile.isShortcut() && !lnkFile.isAlias()
        && !lnkFile.exists()) {
        // This file actually doesn't exist
        logWarning << lnkPath << " doesn't exist in store";
        return ENOENT;
    }

#ifdef Q_OS_WIN
    // Windows shortcuts are like regular files unlike Unix, so needs special handling

    QFile lnkFileHandle{ lnkPath };

    if (!lnkFileHandle.open(QIODevice::ReadOnly)) {
        logWarning << lnkPath << " has inappropriate permissions";
        return EACCES;
    }

    lnkFileHandle.close();
#endif

    const QString lnkTargetPath =
            QDir::cleanPath(QDir{ lnkDirPath }.filePath(lnkFile.symLinkTarget()));
    const QFileInfo lnkTarget{ lnkTargetPath };
    const QString   lnkTargetFileName = lnkTarget.fileName();

    logDebug << "Symlink target: " << lnkTargetPath;

    // All healthy symlinks canonically point to `m_blobDirPath`
    if (lnkTarget.absolutePath() != this->m_blobDirPath) [[unlikely]] {
        // An alien symlink in the store, disallow symlink resolution
        logWarning << lnkPath << " points to outside the store or not a symbolic link";
        return EINVAL;
    }

    if (!blobName.isEmpty() && (lnkTargetFileName != blobName)) {
        logWarning << "Link target " << lnkTargetFileName
                   << " does not match with the provided file name" << blobName;
        return EINVAL;
    }

    return 0;
}

auto FileManPrivate::validateStore() const noexcept -> FileManPrivate::validate_store_t
{
    FileManPrivate::validate_store_t status = { .d_status = -1, .h_status = -1, .c_status = { } };

    for (const QString &dir : { this->m_storeDirPath, this->m_blobDirPath }) {

        const int e         = FileManPrivate::dirHealth(dir);
        int      &dirStatus = (dir == this->m_storeDirPath) ? status.d_status : status.h_status;

        logDebug << dir << " directory health: " << e;

        if (!FileManPrivate::fixDirHealth(dir, e)) {
            dirStatus = e;

            // The checks are serialized based on hierarchy so any errors are fatal, blocking
            // further continuation

            // logDebug << "Error status [d, h, c]: " << status.d_status << ", " << status.h_status
            //          << ", " << status.c_status;

            return status;
        }

        dirStatus = 0;
    }

    for (const auto &[cat, catDirPath] : this->m_categoryToDirs.asKeyValueRange()) {

        logDebug << "Store path (categorized): " << catDirPath;

        const int e = FileManPrivate::dirHealth(catDirPath);

        logDebug << FileManPrivate::sc_categoryToDirNames.value(cat) << " directory health: " << e;

        if (!FileManPrivate::fixDirHealth(catDirPath, e, cat)) {
            status.c_status.insert(cat, e);
            continue;
        }

        status.c_status.insert(cat, 0);
    }

    // logDebug << "Error status [d, h, c]: " << status.d_status << ", " << status.h_status
    //          << ", " << status.c_status;

    return status;
}