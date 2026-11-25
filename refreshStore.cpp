#include <fileman_p.hpp>

auto FileMan::refreshStore() -> int
{
    Q_D(FileMan);

    // Store Validations & Fixups //////////////////////////////////////////////////////////////////

    const auto s      = d->validateStore();
    int        retval = FileManPrivate::compErrnoSeverity(s.d_status, s.h_status);

    for (const int e : s.c_status)
        retval = FileManPrivate::compErrnoSeverity(retval, e);

    if (retval != 0 && retval != EAGAIN) [[unlikely]]
        return retval;

    logInfo << "Store validation succeeded";

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

    // Clear the store cache dictionaries and revoke trust
    d->clearAllEntries();
    d->m_storeManaged = false;

    //----------------------------------------------------------------------------------------------

    //  Store state caching & pruning //////////////////////////////////////////////////////////////

    for (const auto &[cat, catDirPath] : d->m_categoryToDirs.asKeyValueRange()) {

        const QDir categoryDir = catDirPath;

        if (categoryDir.isEmpty(QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot)) {
            logWarning << FileMan::toString(cat) << " store doesn't have any contents yet";
            continue;
        }

        // Enlist all top-level files (oldest first sorted) in the directory. QDir::System is
        // necessary to enlist dangling symlinks
        const QStringList contents = categoryDir.entryList(
                QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot, QDir::Time);

        logDebug << "Contents in " << catDirPath << ":";
        logDebug << contents;

        // Build the store-state cache dictionary from valid files in the store
        for (const QString &file : contents) {
            const QString filePath = categoryDir.absoluteFilePath(file);
            const auto    result   = d->preRmValidate(filePath, true);

            if (result.lnkEntry.isEmpty()) [[unlikely]] {
                if (result.status != ENOENT)
                    logCritical << "Couldn't operate successfully.";

                continue;
            }

            // RHS is rarer than LHS
            if (result.blobEntry.isEmpty() || d->m_blobToLinkDict.contains(result.blobEntry))
                    [[unlikely]] {
                logDebug << "Removing " << file << " from " << catDirPath;

                if (!QFile::remove(filePath)) [[unlikely]]
                    logWarning << "Couldn't remove " << filePath;

                continue;
            }

            d->registerEntry(result.lnkEntry, result.blobEntry);
        }
    }

    d->dumpAllEntries();

    const bool hasValidEntries = !d->isDictEmpty(d->m_blobToLinkDict);

    // Compact the dictionary after finished building, if anything was found
    if (hasValidEntries)
        d->squeezeDicts();

    QDir              blobDir = d->m_blobDirPath;
    const QStringList contents =
            blobDir.entryList(QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);

    if (contents.isEmpty()) {
        if (!hasValidEntries) [[likely]] {
            // Genuinely nothing in the "common" store; our job here is done
            logInfo << "Common store doesn't have any valid contents";

            d->m_storeManaged = true;

            return 0;
        }

        // m_blobToLinkDict has entries but blobDir enlists empty: certain failure, not a
        // legitimately empty store
        logWarning << "Couldn't enlist " << d->m_blobDirPath;

        return EACCES;
    }

    int e = 0;

    for (const QString &blobName : contents) {
        if (d->m_blobToLinkDict.contains(blobName)) [[likely]]
            continue;

        logDebug << "Removing " << blobName << " from " << d->m_blobDirPath;

        if (!blobDir.remove(blobName)) [[unlikely]] {
            logWarning << "Couldn't remove " << blobName;

            if (e == 0)
                e = EACCES;

            continue;
        }
    }

    //----------------------------------------------------------------------------------------------

    //  Finalizing /////////////////////////////////////////////////////////////////////////////////

    d->m_storeManaged = true;

    logInfo << "Housekeeping done on store";

    return e;
}
