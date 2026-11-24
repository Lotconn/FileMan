#include <fileman_p.hpp>

auto FileMan::addFile(const QUrl &fileUrl, bool overwrite, const FileMan::Categories categories)
        -> add_file_t
{
    Q_D(FileMan);

    const bool trustStore = d->m_storeManaged;

    // Store validations first; trust if managed already ///////////////////////////////////////////

    if (!trustStore) {
        const auto s      = d->validateStore();
        int        retval = FileManPrivate::compErrnoSeverity(s.d_status, s.h_status);

        for (const int e : s.c_status)
            retval = FileManPrivate::compErrnoSeverity(retval, e);

        if (retval != 0 && retval != EAGAIN) [[unlikely]]
            return { retval, { }, { } };

        logDebug << "Store validation succeeded";
    }

    //
    //  Exclusively lock the store during accessing, do store operations and auto unlocks once
    //  addFile() goes out-of-scope.
    //

    if (const int e = d->acquireStoreLock(); e != 0)
        return { e, { }, { } };

    // Unlocks automatically once out-of-scope
    const auto unlockGuard = qScopeGuard([&d]() -> void { d->releaseStoreLock(); });

    //----------------------------------------------------------------------------------------------

    //  Argument validations ///////////////////////////////////////////////////////////////////////

    //
    // C++ implementations can check compile-time for invalid enum/flags but unfortunately, no such
    // checks exists for QML. Ensure "categories" is valid before proceeding
    //
    // Also takes care of edge-case Category::None
    //
    if ((categories & ~Categories(Category::All)) != 0U) {
        logCritical << "Provided categories are not valid";
        return { EINVAL, { }, { } };
    }

    //
    // URL                                  =>  Translated path
    // -------------------------------------------------------------------
    // file:thisFileOnCurrentDir.txt        =>  ./thisFileOnCurrentDir.txt
    // file:///path/to/file                 =>  /path/to/file
    // servername:///path/to/file           =>  //servername/path/to/file
    // ftp://servername.com/path/to/folder/ =>  ""
    // /path/to/file                        =>  ""
    //
    const QString fileUrlStr = fileUrl.toDisplayString();
    const QString filePath   = fileUrl.toLocalFile();

    if (filePath.isEmpty()) {
        logCritical << "No such file:" << filePath;
        return { ENOENT, { }, { } };
    }

    const QFileInfo srcFile{ filePath };
    const QString   srcPath     = srcFile.absoluteFilePath();
    const QString   srcFileName = srcFile.fileName();
    QFile           srcFileHandle{ srcPath };

    logDebug << "Resolved file:" << srcPath;

    if (srcPath.startsWith(this->storeDir(), Qt::CaseInsensitive)) {
        logCritical << "Adding files in store is not allowed";
        return { EOPNOTSUPP, { }, { } };
    }

    //----------------------------------------------------------------------------------------------

    //  File validation ////////////////////////////////////////////////////////////////////////////

    const auto result = FileManPrivate::preAddValidate(srcFile);

    logDebug << "File validation result:\n"
             << "Status: " << result.status << '\n'
             << "Category:" << result.categories;

    if (result.status != 0)
        return { result.status, { }, { } };

    if (!categories.testAnyFlags(result.categories)) {
        logCritical << "No valid category found for" << srcPath;
        return { EINVAL, { }, { } };
    }

    //----------------------------------------------------------------------------------------------

    //  Category selection & store operations //////////////////////////////////////////////////////

    const Category catSelected = FileManPrivate::lowestSetFlag(categories & result.categories);

    logDebug << "Selected category:" << FileManPrivate::sc_categoryToString.value(catSelected);

    //----------------------------------------------------------------------------------------------

    //  File & store preparations //////////////////////////////////////////////////////////////////

    if (!d->mkDataDir() || !d->mkDataDir(catSelected)) [[unlikely]] {
        logCritical << "Failed to create required directories";
        return { EPERM, { }, { } };
    }

    if (!srcFileHandle.open(QIODeviceBase::ReadOnly)) {
        logCritical << "Couldn't open" << srcPath << "for reading";
        return { EPERM, { }, { } };
    }

    // Raw data files/blobs reside as dataDir/common/<blob>
    const QDir      blobDir  = d->m_blobDirPath;
    const QString   blobName = FileManPrivate::calcDigest(srcPath);
    const QString   blobPath = blobDir.absoluteFilePath(blobName);
    const QFileInfo blobFile{ blobDir, blobName };

    // Check available (quota) free space before copying. Proceed even if unable to check.
    if (const int e = FileManPrivate::diskFreeCheck(srcFile, blobDir); e != -1 && e != 0)
        return { e, { }, { } };

    QString lnkFileName = srcFileName;
#ifdef Q_OS_WIN
    const QString winLnkExt = u".lnk"_s;

    lnkFileName += winLnkExt;
#endif
    const QString   catDirName = FileManPrivate::sc_categoryToDirNames.value(catSelected);
    const QDir      catDir     = this->categoryDir(catSelected);
    const QFileInfo lnkFile{ catDir, lnkFileName };
    QString         lnkPath  = catDir.absoluteFilePath(lnkFileName);
    QString         lnkEntry = FileManPrivate::createPathComp(catDirName, lnkFileName);

    //----------------------------------------------------------------------------------------------

    //  File deduplication & staging ///////////////////////////////////////////////////////////////

    // Existence checks on symlinks resolve targets instead of the symlinks themselves
    // QFileInfo::isSymlink() is deprecated
    const bool blobExists   = (blobFile.isSymbolicLink() || blobFile.isAlias()
                               || blobFile.isShortcut() || QFile::exists(blobPath));
    const bool lnkExists    = (lnkFile.isSymbolicLink() || lnkFile.isAlias() || lnkFile.isShortcut()
                               || QFile::exists(lnkPath));
    bool       blobIsOrphan = false;

    if (blobExists) {
        if (overwrite) {
            if (!QFile::remove(blobPath)) [[unlikely]] {
                // Certainly NOT because blob doesn't exist in the dir, inhibiting the actual
                // intention for overwrite. Not acceptable.
                logCritical << "Couldn't remove" << blobPath;
                return { EACCES, { }, { } };
            }

            logDebug << blobPath << "removed successfully";
        } else {
            //  Expend fs IO to reverse lookup the related symlink file. Note the output is not
            //  static on initialization, the list can change between calls
            logInfo << fileUrlStr << "already available on store";

            // Search in the broader selected categories, not the filtered catSelected
            if (const auto lookupResult = d->revLookupSymlink(blobName, categories);
                !lookupResult.locations.isEmpty()) [[likely]]
                // In an unlikely case when we get multiple items, just provide the first
                return { EEXIST,
                         QUrl::fromLocalFile(FileManPrivate::createPathComp(
                                 this->storeDir(), lookupResult.locations.first())),
                         blobName };

            logDebug << blobName << "is orphaned in store. Reusing blob...";

            blobIsOrphan = true;
        }
    }

    if (lnkExists) {
        // Overwrite (remove) the symlink only if it was bound to an existing blob
        bool skipRename = (overwrite && blobExists);

        if (skipRename) {
            if (!QFile::remove(lnkPath)) [[unlikely]] {
                logWarning << "Couldn't remove" << lnkPath << ", falling back to renaming";

                skipRename = false;
            } else
                logDebug << lnkPath << "removed successfully";
        }

        if (!skipRename) {
            //
            // Return the present symlink if adding same file with a different name Rename the
            // supplied file with rename-suffix, e.g. existing-file_2.ext and
            // org.munadi.Munadi.png_2.svg (weird naming to begin with). `renameFile()` also
            // automatically handles the path where `lnkEntry` seemingly exists in dict, but doesn't
            // on disk.
            //
            const QString f = FileManPrivate::renameFile(srcFileName, catDir);

            logWarning << "Saving" << srcFileName << "as" << f << "since it already exists";

            lnkFileName = f;
#ifdef Q_OS_WIN
            lnkFileName += winLnkExt;
#endif
            lnkPath  = catDir.absoluteFilePath(lnkFileName);
            lnkEntry = FileManPrivate::createPathComp(catDirName, lnkFileName);
        }
    }

    //----------------------------------------------------------------------------------------------

    // Finalizing //////////////////////////////////////////////////////////////////////////////////

    if (!blobIsOrphan) [[unlikely]] {
        if (!QFile::copy(srcPath, blobPath,
                         QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup
                                 | QFileDevice::ReadOther)) {
            logCritical << "Failed to copy" << srcPath << "to" << blobPath;

            if (QFileInfo::exists(blobPath))
                QFile::remove(blobPath);

            return { EIO, { }, { } };
        }

        logInfo << blobName << "copied successfully to store";
    }

#ifdef Q_OS_WIN
    // .lnk shortcuts resolve relative targets against the app's CWD, not lnkDir; use an absolute
    // target to avoid CWD-dependent resolution.
    const QString targetPath = blobPath;
#else
    const QString targetPath = catDir.relativeFilePath(blobPath);
#endif
    logDebug << "Linking" << lnkPath << "to" << targetPath;

    // Symlink the unanimous blob as the supplied file name under corresponding category
    if (!QFile::link(targetPath, lnkPath)) [[unlikely]] {
        logCritical << "Failed to symlink" << blobPath << "to" << lnkPath;
        return { EACCES, { }, { } };
    }

    logInfo << fileUrlStr << "successfully added";
    logDebug << "Added as" << blobPath << "to store";

    // Register the [ blob, symlink ] entry to `m_blobToLinkDict()`
    logDebug << "Registering blob as " << blobName << " and symlink as" << lnkEntry << " to tables";

    d->registerEntry(lnkEntry, blobName);

    return { 0, QUrl::fromLocalFile(lnkPath), blobName };
}

auto FileMan::addFile(const QUrl &fileUrl, const Categories categories) -> add_file_t
{
    return this->addFile(fileUrl, false, categories);
}