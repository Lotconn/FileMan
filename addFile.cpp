#include <QScopeGuard>
#include <QUuid>

#include "fileman_p.hpp"
#include "internal-helpers.hpp"

//
// [Public] Add file with the provided `fileUrl` to store corresponding to
// supplied `categories`. Returns a `add_file_t` struct having {int status(),
// QUrl location()} members. `status()` is commonly errno:{0, EACCES, EBUSY,
// EEXIST, EINVAL, EIO, ENOENT, EPERM} but could extend beyond the usual.
// `location()` is the fully-qualified local url of the saved file in store, or
// empty on error.
//
add_file_t FileMan::addFile(const QUrl &fileUrl,
                            const FileMan::Categories categories) {

  const bool trustStore = this->d->storeManaged;

  // Store validation first; trust if managed already //////////////////////////

  if (!trustStore) {
    const validate_store_t s = this->validateStore();
    int retval = compErrnoSeverity(s.d_status, s.h_status);

    for (const int e : s.c_status)
      retval = compErrnoSeverity(retval, e);

    if (retval != 0 && retval != EAGAIN)
      return {retval, {}};
  }

  //
  //  Exclusively lock the store during accessing, do store operations and
  //  auto unlocks once addFile() goes out-of-scope.
  //

  if (const int e = FileMan::acquireStoreLock(); e != 0)
    return {e, {}};

  // Unlocks automatically once out-of-scope
  const auto unlockGuard = qScopeGuard([this] { releaseStoreLock(); });

  //----------------------------------------------------------------------------

  //  Argument validations /////////////////////////////////////////////////////

  //
  // C++ implementations can check compile-time for invalid enum/flags
  // but unfortunately, no such checks exists for QML. Ensure "category"
  // is valid before proceeding
  //
  // Also takes care of edge-case FileMan::None
  //
  if ((categories & ~FileMan::All) != 0U) {
    logCritical << "Provided categories are not valid";
    return {EINVAL, {}};
  }

  //
  // URL                                  =>  Translated path
  // -----------------------------------------------------------------------
  // file:thisFileOnCurrentDir.txt        =>  ./thisFileOnCurrentDir.txt
  // file:///path/to/file                 =>  /path/to/file
  // servername:///path/to/file           =>  //servername/path/to/file
  // ftp://servername.com/path/to/folder/ =>  ""
  // /path/to/file                        =>  ""
  //
  const QString &fileUrlStr = fileUrl.toDisplayString();
  const QString &filePath = fileUrl.toLocalFile();

  if (filePath.isEmpty()) {
    logCritical << "No such file:" << filePath;
    return {ENOENT, {}};
  }

  const QString &srcPath = QFileInfo(filePath).absoluteFilePath();
  const QFileInfo srcFile(srcPath);
  const QString &srcFileName = srcFile.fileName();

  logDebug << "Resolved file:" << srcPath;

  //----------------------------------------------------------------------------

  //  File validation //////////////////////////////////////////////////////////

  const pre_add_validate_t &result = preAddValidate(srcFile);

  logDebug << "File validation result:\n"
           << "Status: " << result.status << '\n'
           << "Category:" << result.categories;

  if (result.status != 0)
    return {result.status, {}};

  if (!categories.testAnyFlags(result.categories)) {
    logCritical << "No valid category found for" << srcPath;
    return {EINVAL, {}};
  }

  //----------------------------------------------------------------------------

  //  Category selection & store operations ////////////////////////////////////

  FileMan::Category catSelected = lowestSetFlag(categories & result.categories);
  const QString categoryName = this->d->categoryList.value(catSelected).name;

  logDebug << "Selected category:" << categoryName;

  //----------------------------------------------------------------------------

  //  File & store preparations ////////////////////////////////////////////////

  if (!mkDataDir() || !mkDataDir(catSelected))
    return {ENOENT, {}};

  //
  // Hash the file to get unanimous identity, stage `srcFile` in the same read
  // pass. This avoids a second redundant full read on the same file for the
  // fallback-case copying path. Final name isn't known until hashed, so stage
  // it under a temporary name first, then commit/rename to `destPath` after
  // hardlinking fails.
  //

  // Raw data files/blobs reside as dataDir/common/<hash>
  const QDir hashDir(this->d->hashDir);

  const QString tmpFileName =
      ".fileman-"_L1 + QUuid::createUuid().toString(QUuid::Id128) + ".part"_L1;
  const QString tmpPath = hashDir.absoluteFilePath(tmpFileName);

  QFile srcFileHandle(srcPath);
  QSaveFile tmpFile(tmpPath);

  // Open both `srcFileHandle` & `tmpFile` for passing to `hashAndStageFile()`

  if (!srcFileHandle.open(QIODeviceBase::ReadOnly)) {
    logCritical << "Couldn't open" << srcPath << "for reading";

    return {EPERM, {}};
  }

  // Check available (quota) free space before copying. Proceed even if unable
  // to check.
  if (const int e = diskFreeCheck(srcFile, hashDir); e != -1 && e != 0)
    return {e, {}};

  if (!tmpFile.open(QIODeviceBase::WriteOnly)) {
    logCritical << "Couldn't open" << tmpPath << "for writing";

    return {EIO, {}};
  }

  const hash_and_stage_t &stage_result =
      hashAndStageFile(srcFileHandle, tmpFile);

  if (stage_result.status != 0)
    return {stage_result.status, {}};

  const QString &hash = stage_result.hash;
  const QString &hashPath = hashDir.absoluteFilePath(hash);
  const QDir &lnkDir(this->d->categoryList.value(catSelected).location);

  QString lnkFileName = srcFileName;
#ifdef Q_OS_WIN
  constexpr QLatin1StringView winLnkExt(".lnk");
  lnkFileName += winLnkExt;
#endif
  QString lnkPath = lnkDir.absoluteFilePath(lnkFileName);
  QString lnkEntry = FileMan::createPathComp(categoryName, lnkFileName);

  //----------------------------------------------------------------------------

  //  File deduplication & staging /////////////////////////////////////////////

  const bool hashExists = hashDir.exists(hash);
  const bool lnkExists = lnkDir.exists(lnkFileName);
  bool hashOrphaned = false;

  if (hashExists) {

    //  Expend fs IO to reverse lookup the related symlink file. Note the
    //  output is not static on initialization, the list can change between
    //  calls
    logInfo << fileUrlStr << "already available on store";

    const FileMan::rev_lookup_symlink_t &lnk =
        this->revLookupSymlink(lnkDir, hashPath);

    if (lnk.status == EPERM) {
      tmpFile.cancelWriting();
      logCritical
          << "Store is not accessible. Execuatble permission is required";

      return {lnk.status, {}};
    }

    if (!lnk.location.isEmpty()) {
      tmpFile.cancelWriting();

      return {EEXIST, QUrl::fromLocalFile(lnk.location)};
    }

    logDebug << hash << "is orphaned in store. Reusing blob...";

    hashOrphaned = true;
  }

  if (lnkExists) {

    //
    // Return the present symlink if adding same file with a different name
    // Rename the supplied file with rename-suffix, e.g. existing-file_2.ext
    // and org.munadi.Munadi.png_2.svg (weird naming to begin with).
    // `renameFile()` also automatically handles the path where `lnkEntry`
    // seemingly exists in dict, but doesn't on disk.
    //
    const QString &f = renameFile(srcFileName, lnkDir);

    logWarning << "Saving" << srcFileName << "as" << f
               << "since it already exists";

    lnkFileName = f;
#ifdef Q_OS_WIN
    lnkFileName += winLnkExt;
#endif
    lnkPath = lnkDir.absoluteFilePath(lnkFileName);
    lnkEntry = FileMan::createPathComp(categoryName, lnkFileName);
  }

  // Opportunistically try hardlinking the file, fallback to committing the
  // already-staged copy if fails. Skips entirely if a matching orphaned blob
  // already exists in store
  bool hlStatus = false;

  if (!hashOrphaned) {
    std::error_code ec;
    std::filesystem::create_hard_link(srcPath.toStdString(),
                                      hashPath.toStdString(), ec);
    if (ec.value() == 0) {
      logDebug << "Hardlinked" << srcPath << "to" << hashPath;
      hlStatus = true;
    } else {

      // EFAULT, EPERM & EXDEV are values for usual hardlinking errors
      if (ec != std::errc::cross_device_link && ec != std::errc::bad_address &&
          ec != std::errc::operation_not_permitted) {
        logCritical << "Problem occured during hardlinking, can't operate";

        tmpFile.cancelWriting();
        return {ec.value(), {}};
      }

      logWarning << "Hardlinking failed, falling back to regular copy";
      logDebug << "Error Code:" << ec.value();
    }
  }

  if (hlStatus || hashOrphaned)
    // Discard the staged copy
    tmpFile.cancelWriting();

  //----------------------------------------------------------------------------

  // Finalizing ////////////////////////////////////////////////////////////////

  else { // Commit the staged `tmpFile` as final hashed blob

    if (!tmpFile.commit()) {
      logCritical << "Failed to commit staged file" << tmpFile.errorString();

      return {EIO, {}};
    }

    if (!QFile::rename(tmpPath, hashPath)) {
      logCritical << "Failed to finalize staged file as" << hashPath;

      return {EIO, {}};
    }
  }

#ifdef Q_OS_WIN
  // .lnk shortcuts resolve relative targets against the app's CWD, not
  // lnkDir; use an absolute target to avoid CWD-dependent resolution.
  const QString &targetPath = hashPath;
#else
  const QString &targetPath = lnkDir.relativeFilePath(hashPath);
#endif
  logDebug << "Linking" << lnkPath << "to" << targetPath;

  // Symlink the unanimous hashed file as the supplied file name under
  // corresponding category
  if (!QFile::link(targetPath, lnkPath)) {
    logCritical << "Failed to symlink" << hashPath << "to" << lnkPath;

    return {EACCES, {}};
  }

  logInfo << fileUrlStr << "successfully added";
  logDebug << "Added as" << hashPath << "to store";

  // Register the [ hash, symlink ] entry to `hashToLinkDict()`
  logDebug << "Registering hash as " << hash << " and symlink as" << lnkEntry
           << " to tables";

  this->registerEntry(lnkEntry, hash);

  return {0, QUrl::fromLocalFile(lnkPath)};
}