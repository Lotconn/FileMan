#include <fileman_p.hpp>

auto FileMan::rmFile(const QUrl &fileUrl) -> rm_file_t
{
    Q_D(FileMan);

    const bool trustStore = d->storeManaged;

    // Store validation first; trust if managed already ////////////////////////////////////////////

    if (!trustStore) {
        const auto s      = d->validateStore();
        int        retval = FileManPrivate::compErrnoSeverity(s.d_status, s.h_status);

        for (const int e : s.c_status)
            retval = FileManPrivate::compErrnoSeverity(retval, e);

        if (retval != 0 && retval != EAGAIN) [[unlikely]]
            return { retval, -1 };
    }

    //
    //  Exclusively lock the store during accessing, do store operations and auto unlocks once
    //  rmFile() goes out-of-scope.
    //

    if (const int e = d->acquireStoreLock(); e != 0)
        return { e, -1 };

    // Unlocks automatically once out-of-scope
    const auto unlockGuard = qScopeGuard([&d]() -> void { d->releaseStoreLock(); });

    //----------------------------------------------------------------------------------------------

    //  File validations ///////////////////////////////////////////////////////////////////////////

    //
    // URL                                  =>  Translated path
    // ---------------------------------------------------------------------------------------------
    // file:thisFileOnCurrentDir.txt        =>  ./thisFileOnCurrentDir.txt
    // file:///path/to/file                 =>  /path/to/file
    // servername:///path/to/file           =>  //servername/path/to/file
    // ftp://servername.com/path/to/folder/ =>  ""
    // /path/to/file                        =>  ""
    //
    const QString fileUrlStr = fileUrl.toDisplayString();
    const QString lnkPath = QDir::cleanPath(QFileInfo{ fileUrl.toLocalFile() }.absoluteFilePath());

    const auto validateResult = d->preRmValidate(lnkPath);

    if (validateResult.status != 0) {
        logCritical << "Refusing to proceed with" << fileUrlStr;
        return { validateResult.status, -1 };
    }

    //----------------------------------------------------------------------------------------------

    //  File Operations ////////////////////////////////////////////////////////////////////////////

    const QFileInfo lnkFile{ lnkPath };
    const QString   lnkFileName = lnkFile.fileName();
    QDir            blobDir     = d->blobDirPath;
    int             status_b    = 0;
    int             status_l    = 0;
    bool            blobRemoved = true;
    bool            lnkRemoved  = false;

    // Remove the raw blob from the store first
    if (validateResult.blobEntry.isEmpty()) [[unlikely]]
        logWarning << "No valid blobs found in store for" << fileUrlStr;
    else {
        // Let's trust that preRmValidate() won't return orphaned blobs
        if (const auto lookupResult = d->revLookupSymlink(validateResult.blobEntry);
            lookupResult.locations.size() > 1) {

            logWarning << "Multiple attached symlinks found for" << validateResult.blobEntry;
            logWarning << "Refusing to remove blob";
            status_b = ENOTUNIQ;
        } else if (blobRemoved = blobDir.remove(validateResult.blobEntry); !blobRemoved)
                [[unlikely]] {
            logCritical << "Couldn't remove" << validateResult.blobEntry;
            status_b = EACCES;
        }
    }

    // Remove the symlink next
    if (lnkRemoved = QFile::remove(lnkPath); !lnkRemoved) [[unlikely]] {
        logCritical << "Couldn't remove" << fileUrlStr;
        status_l = EACCES;
    }

    //----------------------------------------------------------------------------------------------

    //  Finalizing /////////////////////////////////////////////////////////////////////////////////

    // Unregister the entries from the hash table
    d->removeEntry(Q_LIKELY(lnkRemoved) ? validateResult.lnkEntry : ""_L1,
                   Q_LIKELY(blobRemoved) ? validateResult.blobEntry : ""_L1);

    if ((status_l == 0) && (status_b == 0)) [[likely]]
        logInfo << fileUrlStr << "removed successfully from store";
    else [[unlikely]]
        logInfo << fileUrlStr << "removal from store wasn't fully successful";

    return { status_l, status_b };
}

auto FileMan::rmFile(const QString &fileId) -> rm_file_t
{
    Q_D(FileMan);

    const bool trustStore = d->storeManaged;

    // Store validation first; trust if managed already ////////////////////////////////////////////

    if (!trustStore) {
        const auto s      = d->validateStore();
        int        retval = FileManPrivate::compErrnoSeverity(s.d_status, s.h_status);

        for (const int e : s.c_status)
            retval = FileManPrivate::compErrnoSeverity(retval, e);

        if (retval != 0 && retval != EAGAIN) [[unlikely]]
            return { retval, -1 };
    }

    //
    //  Exclusively lock the store during accessing, do store operations and auto unlocks once
    //  rmFile() goes out-of-scope.
    //

    if (const int e = d->acquireStoreLock(); e != 0)
        return { e, -1 };

    // Unlocks automatically once out-of-scope
    const auto unlockGuard = qScopeGuard([&d]() -> void { d->releaseStoreLock(); });

    //----------------------------------------------------------------------------------------------

    QFileInfo     blobFile{ fileId };
    const QString blobFileName = blobFile.fileName();
    const QString blobPath     = FileManPrivate::createPathComp(d->blobDirPath, blobFileName);

    if (blobFileName.isEmpty()) {
        logCritical << fileId << "is not a valid file";
        return { -1, EINVAL };
    }

    blobFile.setFile(blobPath);

    if (const int e = d->validateBlob(blobFile); e != 0)
        return { -1, e };

    const auto lookupResult = d->revLookupSymlink(blobFileName);
    bool       blobRemoved  = false;
    int        status_l     = 0;

    logInfo << "Removing" << blobPath;

    // Remove the blob first
    if (blobRemoved = QFile::remove(blobPath); !blobRemoved) [[unlikely]] {
        logCritical << "Couldn't remove" << fileId;
        return { -1, EACCES };
    }

    if (lookupResult.status == ENOENT) [[unlikely]] {
        logInfo << fileId << "was an orphan in store";
        return { -1, 0 };
    }

    // Remove all the symlinks pointing to it next
    for (const QString &lnkEntry : lookupResult.locations) {
        const QString lnkPath = FileManPrivate::createPathComp(d->storeDirPath, lnkEntry);

        // Do not remove invalid reverse-resolved symlinks. Pedantic, not really necessary;
        // revLookupSymlink() already did the job, hopefully in a good manner
        if (d->validateLink(QFileInfo{ lnkPath }, blobFileName) != 0) [[unlikely]]
            continue;

        logInfo << "Removing" << lnkPath;

        if (!QFile::remove(lnkPath)) [[unlikely]] {
            logCritical << "Couldn't remove" << lnkPath;

            if (status_l == 0)
                status_l = EACCES;
        }

        // Also unregister the entries from the hash table. Failed-to-remove links are already stale
        // anyway
        d->removeEntry(lnkEntry, blobFileName);
    }

    //----------------------------------------------------------------------------------------------

    //  Finalizing /////////////////////////////////////////////////////////////////////////////////

    logInfo << fileId << "successfully removed from store";

    return { 0, 0 };
}