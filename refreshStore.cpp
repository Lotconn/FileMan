#include <fileman_p.hpp>

auto FileMan::refreshStore() -> int
{
    using pre_rm_validate_t = FileManPrivate::pre_rm_validate_t;
    using validate_store_t  = FileManPrivate::validate_store_t;
    Q_D(FileMan);

    // Store Validations & Fixups //////////////////////////////////////////////////////////////////

    const validate_store_t s      = d->validateStore();
    int                    retval = FileManPrivate::compErrnoSeverity(s.d_status, s.h_status);

    for (const int e : s.c_status)
        retval = FileManPrivate::compErrnoSeverity(retval, e);

    if (retval != 0 && retval != EAGAIN) [[unlikely]]
        return retval;

    //----------------------------------------------------------------------------------------------

    //  Preparations ///////////////////////////////////////////////////////////////////////////////

    //
    //  Exclusively lock the store during accessing, do store operations and auto unlocks once
    //  refreshStore() goes out-of-scope.
    //

    if (const int e = d->acquireStoreLock(); e != 0)
        return e;

    // Unlocks automatically once out-of-scope
    const auto unlockGuard = qScopeGuard([&d]() -> void { d->releaseStoreLock(); });

    // Clear the store cache dictionaries
    d->clearAllEntries();

    //----------------------------------------------------------------------------------------------

    //  Store state caching & pruning //////////////////////////////////////////////////////////////

    for (const auto &[category, catInfo] : d->categoryList.asKeyValueRange()) {

        // Since we can return only an int, we return the value of worst error that occured during
        // `refreshStore()`'s lifetime. Use `compErrnoSeverity()` for the comparison

        QDir categoryDir = catInfo.location;

        if (categoryDir.isEmpty()) {
            logWarning << catInfo.name << "store doesn't have any contents yet";
            continue;
        }

        // Enlist all top-level files (unsorted) in the directory. QDir::System is necessary to
        // enlist dangling symlinks
        const QStringList contents =
                categoryDir.entryList(QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);

        logDebug << "Contents in" << catInfo.location << ":";
        logDebug << contents;

        // Build the store-state cache dictionary from valid files in the store
        for (const QString &file : contents) {
            const QString           filePath = categoryDir.absoluteFilePath(file);
            const pre_rm_validate_t result   = d->preRmValidate(filePath, true);

            if (result.lnkEntry.isEmpty()) [[unlikely]] {
                if (result.status != ENOENT)
                    logCritical << "Couldn't operate successfully.";

                continue;
            }

            if (result.hashEntry.isEmpty()) [[unlikely]] {
                logDebug << "Removing" << file << "from" << catInfo.location;

                if (!categoryDir.remove(file)) [[unlikely]]
                    logWarning << "Couldn't remove" << file;

                continue;
            }

            d->registerEntry(result.lnkEntry, result.hashEntry);
        }
    }

    d->dumpAllEntries();

    const bool hasValidEntries = !d->isDictEmpty(d->hashToLinkDict);

    // Compact the dictionary after finished building, if anything was found
    if (hasValidEntries)
        d->squeezeDicts();

    QDir hashDir = d->hashDir;

    const QStringList contents =
            hashDir.entryList(QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);

    if (contents.isEmpty()) {
        if (!hasValidEntries) [[likely]] {
            // Genuinely nothing in the "common" store; our job here is done
            logInfo << "Common store doesn't have any valid contents";

            d->storeManaged = true;

            return 0;
        }

        // hashToLinkDict has entries but hashDir enlists empty: certain failure, not a legitimately
        // empty store
        logWarning << "Couldn't enlist" << d->hashDir;

        return EACCES;
    }

    int e = 0;

    for (const QString &hash : contents) {
        if (d->hashToLinkDict.contains(hash)) [[likely]]
            continue;

        logDebug << "Removing" << hash << "from" << d->hashDir;

        if (!hashDir.remove(hash)) [[unlikely]] {
            logWarning << "Couldn't remove" << hash;

            if (e == 0)
                e = EACCES;

            continue;
        }
    }

    //----------------------------------------------------------------------------------------------

    //  Finalizing /////////////////////////////////////////////////////////////////////////////////

    d->storeManaged = true;

    logInfo << "Housekeeping done on store";

    return e;
}
