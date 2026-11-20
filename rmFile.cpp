#include <fileman_p.hpp>

auto FileMan::rmFile(const QUrl &fileUrl) -> rm_file_t
{
    using pre_rm_validate_t = FileManPrivate::pre_rm_validate_t;
    using validate_store_t  = FileManPrivate::validate_store_t;
    Q_D(FileMan);

    const bool trustStore = d->storeManaged;

    // Store validation first; trust if managed already ////////////////////////////////////////////

    if (!trustStore) {
        const validate_store_t s      = d->validateStore();
        int                    retval = FileManPrivate::compErrnoSeverity(s.d_status, s.h_status);

        for (const int e : s.c_status)
            retval = FileManPrivate::compErrnoSeverity(retval, e);

        if (retval != 0 && retval != EAGAIN) [[unlikely]]
            return { retval, { } };
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
    const QString lnkFilePath =
            QDir::cleanPath(QFileInfo{ fileUrl.toLocalFile() }.absoluteFilePath());
    const QFileInfo lnkFile{ lnkFilePath };
    const QString   lnkFileName = lnkFile.fileName();

    const pre_rm_validate_t v_result = d->preRmValidate(lnkFilePath);

    if (v_result.status != 0) {
        logCritical << "Refusing to proceed with" << fileUrlStr;
        return { v_result.status, -1 };
    }

    //----------------------------------------------------------------------------------------------

    //  File Operations ////////////////////////////////////////////////////////////////////////////

    QDir hashDir     = d->hashDir;
    QDir lnkDir      = lnkFile.dir();
    int  status_h    = 0;
    int  status_l    = 0;
    bool hashRemoved = true;
    bool lnkRemoved  = false;

    // Remove the raw blob from the store first
    if (v_result.hashEntry.isEmpty()) [[unlikely]]
        logWarning << "No valid hashed file found in store for" << fileUrlStr;
    else if (hashRemoved = hashDir.remove(v_result.hashEntry); !hashRemoved) [[unlikely]] {
        logCritical << "Couldn't remove" << v_result.hashEntry;
        status_h = EACCES;
    }

    // Remove the symlink next
    if (lnkRemoved = lnkDir.remove(lnkFileName); !lnkRemoved) [[unlikely]] {
        logCritical << "Couldn't remove" << fileUrlStr;
        status_l = EACCES;
    }

    //----------------------------------------------------------------------------------------------

    //  Finalizing /////////////////////////////////////////////////////////////////////////////////

    // Unregister the entries from the hash table
    d->removeEntry(Q_LIKELY(lnkRemoved) ? v_result.lnkEntry : ""_L1,
                   Q_LIKELY(hashRemoved) ? v_result.hashEntry : ""_L1);

    if ((status_l == 0) && (status_h == 0)) [[likely]]
        logInfo << fileUrlStr << "removed successfully from store";
    else [[unlikely]]
        logInfo << fileUrlStr << "removal from store wasn't fully successful";

    return { status_l, status_h };
}