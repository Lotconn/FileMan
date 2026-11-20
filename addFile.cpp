#include <fileman_p.hpp>

auto FileMan::addFile(const QUrl &fileUrl, bool overwrite, const Categories categories)
        -> add_file_t
{
    using pre_add_validate_t   = FileManPrivate::pre_add_validate_t;
    using rev_lookup_symlink_t = FileManPrivate::rev_lookup_symlink_t;
    using validate_store_t     = FileManPrivate::validate_store_t;
    Q_D(FileMan);

    const bool trustStore = d->storeManaged;

    // Store validations first; trust if managed already ///////////////////////////////////////////

    if (!trustStore) {
        const validate_store_t s      = d->validateStore();
        int                    retval = FileManPrivate::compErrnoSeverity(s.d_status, s.h_status);

        for (const int e : s.c_status)
            retval = FileManPrivate::compErrnoSeverity(retval, e);

        if (retval != 0 && retval != EAGAIN) [[unlikely]]
            return { retval, { }, { } };
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

    const QString   srcPath = QFileInfo{ filePath }.absoluteFilePath();
    const QFileInfo srcFile{ srcPath };
    const QString   srcFileName = srcFile.fileName();
    QFile           srcFileHandle{ srcPath };

    logDebug << "Resolved file:" << srcPath;

    //----------------------------------------------------------------------------------------------

    //  File validation ////////////////////////////////////////////////////////////////////////////

    const pre_add_validate_t result = d->preAddValidate(srcFile);

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

    const Category catSelected  = FileManPrivate::lowestSetFlag(categories & result.categories);
    const QString  categoryName = d->categoryList.value(catSelected).name;

    logDebug << "Selected category:" << categoryName;

    //----------------------------------------------------------------------------------------------

    //  File & store preparations //////////////////////////////////////////////////////////////////

    if (!d->mkDataDir() || !d->mkDataDir(catSelected)) [[unlikely]]
        return { ENOENT, { }, { } };

    if (!srcFileHandle.open(QIODeviceBase::ReadOnly)) {
        logCritical << "Couldn't open" << srcPath << "for reading";
        return { EPERM, { }, { } };
    }

    // Raw data files/blobs reside as dataDir/common/<blob>
    QDir            blobDir  = d->blobDir;
    const QString   blob     = FileManPrivate::calcDigest(srcPath);
    const QString   blobPath = blobDir.absoluteFilePath(blob);
    const QFileInfo blobFile{ blobPath };

    // Check available (quota) free space before copying. Proceed even if unable to check.
    if (const int e = FileManPrivate::diskFreeCheck(srcFile, blobDir); e != -1 && e != 0)
        return { e, { }, { } };

    QString lnkFileName = srcFileName;
#ifdef Q_OS_WIN
    const QString winLnkExt = u".lnk"_s;

    lnkFileName += winLnkExt;
#endif
    QDir            lnkDir   = d->categoryList.value(catSelected).location;
    QString         lnkPath  = lnkDir.absoluteFilePath(lnkFileName);
    QString         lnkEntry = FileManPrivate::createPathComp(categoryName, lnkFileName);
    const QFileInfo lnkFile{ lnkPath };

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
            if (!blobDir.remove(blob)) [[unlikely]] {
                // Certainly NOT because blob doesn't exist in the dir, inhibiting the actual
                // intention for overwrite. Not acceptable.
                logCritical << "Couldn't remove" << blobPath;
                return { EACCES, { }, { } };
            }
        } else {
            //  Expend fs IO to reverse lookup the related symlink file. Note the  output is not
            //  static on initialization, the list can change between calls
            logInfo << fileUrlStr << "already available on store";

            const rev_lookup_symlink_t lnk = d->revLookupSymlink(lnkDir, blobPath);

            if (lnk.status == EPERM) [[unlikely]] {
                logCritical << "Store is not accessible. Execuatble permission is required";
                return { lnk.status, { }, { } };
            }

            if (!lnk.location.isEmpty()) [[likely]]
                return { EEXIST, QUrl::fromLocalFile(lnk.location), blob };

            logDebug << blob << "is orphaned in store. Reusing blob...";

            blobIsOrphan = true;
        }
    }

    if (lnkExists) {
        // Overwrite (remove) the symlink only if it was bound to an existing blob
        bool skipRename = (overwrite && blobExists);

        if (!skipRename && !lnkDir.remove(lnkFileName)) [[unlikely]] {
            logWarning << "Couldn't remove" << lnkPath << ", falling back to renaming";

            skipRename = false;
        }

        if (!skipRename) {
            //
            // Return the present symlink if adding same file with a different name Rename the
            // supplied file with rename-suffix, e.g. existing-file_2.ext and
            // org.munadi.Munadi.png_2.svg (weird naming to begin with). `renameFile()` also
            // automatically handles the path where `lnkEntry` seemingly exists in dict, but doesn't
            // on disk.
            //
            const QString f = FileManPrivate::renameFile(srcFileName, lnkDir);

            logWarning << "Saving" << srcFileName << "as" << f << "since it already exists";

            lnkFileName = f;
#ifdef Q_OS_WIN
            lnkFileName += winLnkExt;
#endif
            lnkPath  = lnkDir.absoluteFilePath(lnkFileName);
            lnkEntry = FileManPrivate::createPathComp(categoryName, lnkFileName);
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
                blobDir.remove(blob);

            return { EIO, { }, { } };
        }
    }

#ifdef Q_OS_WIN
    // .lnk shortcuts resolve relative targets against the app's CWD, not lnkDir; use an absolute
    // target to avoid CWD-dependent resolution.
    const QString targetPath = blobPath;
#else
    const QString targetPath = lnkDir.relativeFilePath(blobPath);
#endif
    logDebug << "Linking" << lnkPath << "to" << targetPath;

    // Symlink the unanimous blob as the supplied file name under corresponding category
    if (!QFile::link(targetPath, lnkPath)) [[unlikely]] {
        logCritical << "Failed to symlink" << blobPath << "to" << lnkPath;
        return { EACCES, { }, { } };
    }

    logInfo << fileUrlStr << "successfully added";
    logDebug << "Added as" << blobPath << "to store";

    // Register the [ blob, symlink ] entry to `blobToLinkDict()`
    logDebug << "Registering blob as " << blob << " and symlink as" << lnkEntry << " to tables";

    d->registerEntry(lnkEntry, blob);

    return { 0, QUrl::fromLocalFile(lnkPath), blob };
}

auto FileMan::addFile(const QUrl &fileUrl, const Categories categories) -> add_file_t
{
    return this->addFile(fileUrl, false, categories);
}