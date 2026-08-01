#include "fileman_p.hpp"
#include "internal-helpers.hpp"

//
// [Public] A housekeeper for the store that builds store-state cache
// dictionaries, prunes orphan hashed blobs and dangling symlinks and attempts
// fix for directory permissions or missing required directories. Should run on
// application/component startup.
//
int FileMan::refreshStore() {

  // Store Validations & Fixups ////////////////////////////////////////////////

  const validate_store_t s = this->validateStore();
  int retval = compErrnoSeverity(s.d_status, s.h_status);

  for (const int e : s.c_status)
    retval = compErrnoSeverity(retval, e);

  if (retval != 0 && retval != EAGAIN)
    return retval;

  const QFileInfo dataDir(d->dataDir);

  //----------------------------------------------------------------------------

  //  Preparations /////////////////////////////////////////////////////////////

  //
  //  Exclusively lock the store during accessing, do store operations and
  //  auto unlocks once refreshStore() goes out-of-scope.
  //

  if (const int e = this->acquireStoreLock(); e != 0)
    return e;

  // Unlocks automatically once out-of-scope
  const auto unlockGuard = qScopeGuard([this] { releaseStoreLock(); });

  // Clear the store cache dictionaries
  this->clearAllEntries();

  //--------------------------------------------------------------------------

  //  Store state caching & pruning ////////////////////////////////////////////

  for (const auto &[category, catInfo] : d->categoryList.asKeyValueRange()) {

    // Since we can return only an int, we return the value of worst error that
    // occured during `refreshStore()`'s lifetime. Use
    // `compErrnoSeverity()` for the comparison

    QDir categoryDir(catInfo.location);

    if (categoryDir.isEmpty()) {
      logWarning << catInfo.name << "store doesn't have any contents yet";

      continue;
    }

    // Enlist all top-level files (unsorted) in the directory. QDir::System is
    // necessary to enlist dangling symlinks
    const QStringList contents = categoryDir.entryList(
        QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);

    logDebug << "Contents in" << catInfo.location << ":";
    logDebug << contents;

    // Build the store-state cache dictionary from valid files in the store
    for (const QString &file : contents) {

      const QString filePath = categoryDir.absoluteFilePath(file);
      const pre_rm_validate_t result = preRmValidate(filePath, true);

      if (result.lnkEntry.isEmpty()) {
        if (result.status == ENOENT)
          logWarning << file << "is not available in store";
        else
          logCritical << "Couldn't operate successfully.";

        continue;
      }

      if (result.hashEntry.isEmpty()) {
        if (!categoryDir.remove(file))
          logWarning << "Couldn't remove" << file;

        continue;
      }

      this->registerEntry(result.lnkEntry, result.hashEntry);
    }
  }

  this->dumpAllEntries();

  const bool hasValidEntries = !isDictEmpty(d->hashToLinkDict);

  // Compact the dictionary after finished building, if anything was found
  if (hasValidEntries)
    this->squeezeDicts();

  QDir hashDir(d->hashDir);

  const QStringList contents =
      hashDir.entryList(QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);

  if (contents.isEmpty()) {
    if (!hasValidEntries) {
      // Genuinely nothing in the "common" store; our job here is done
      logInfo << "Common store doesn't have any valid contents";

      d->storeManaged = true;

      return 0;
    }

    // hashToLinkDict has entries but hashDir enlists empty: certain failure,
    // not a legitimately empty store
    logWarning << "Couldn't enlist" << d->hashDir;

    return EACCES;
  }

  int e = 0;

  for (const QString &hash : contents)
    if (!d->hashToLinkDict.contains(hash))
      if (!hashDir.remove(hash)) {
        logWarning << "Couldn't remove" << hash;

        if (e == 0)
          e = EACCES;

        continue;
      }

  //----------------------------------------------------------------------------

  //  Finalizing ///////////////////////////////////////////////////////////////

  d->storeManaged = true;

  logInfo << "Housekeeping done on store";

  return e;
}
