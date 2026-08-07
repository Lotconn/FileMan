#include "fileman_p.hpp"
#include "internal-helpers.hpp"

//
// [Public] Remove file with provided `fileUrl` from store. Returns a
// `rm_file_t` struct having {int status_l(), int status_h()} members.
// `status_h()` is -1 when encountering failure early before hash blob is
// checked, otherwise is errno or 0 on success. `status_l()` is 0 on success or
// errno on symlink-check failures.
//
rm_file_t FileMan::rmFile(const QUrl &fileUrl) {

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
  //  auto unlocks once rmFile() goes out-of-scope.
  //

  if (const int e = this->acquireStoreLock(); e != 0)
    return {e, -1};

  // Unlocks automatically once out-of-scope
  const auto unlockGuard = qScopeGuard([this] { releaseStoreLock(); });

  //----------------------------------------------------------------------------

  //  File validations /////////////////////////////////////////////////////////

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
  const QString &lnkFilePath =
      QDir::cleanPath(QFileInfo(fileUrl.toLocalFile()).absoluteFilePath());
  const QFileInfo lnkFile(lnkFilePath);
  const QString &lnkFileName = lnkFile.fileName();
  const QString &lnkDirName = lnkFile.dir().dirName(); // i.e. parent dir name

  const FileMan::pre_rm_validate_t v_result = preRmValidate(lnkFilePath);

  if (v_result.status != 0) {
    logCritical << "Refusing to proceed with" << fileUrlStr;

    return {v_result.status, -1};
  }

  //----------------------------------------------------------------------------

  //  File Operations //////////////////////////////////////////////////////////

  int status_l = 0;
  int status_h = 0;
  QDir hashDir(this->d->hashDir);
  QDir lnkDir = lnkFile.dir();

  bool removeHash = true;
  bool removeLnk = false;

  // Remove the raw blob from the store first
  if (v_result.hashEntry.isEmpty())
    logWarning << "No valid hashed file found in store for" << fileUrlStr;
  else if (removeHash = hashDir.remove(v_result.hashEntry); !removeHash) {

    logCritical << "Couldn't remove" << v_result.hashEntry;

    status_h = EACCES;
  }

  // Remove the symlink next
  if (removeLnk = lnkDir.remove(lnkFileName); !removeLnk) {
    logCritical << "Couldn't remove" << fileUrlStr;

    status_l = EACCES;
  }

  //----------------------------------------------------------------------------

  //  Finalizing ///////////////////////////////////////////////////////////////

  // Unregister the entries from the hash table
  removeEntry((removeLnk) ? v_result.lnkEntry : ""_L1,
              (removeHash) ? v_result.hashEntry : ""_L1);

  if ((status_l == 0) && (status_h == 0))
    logInfo << fileUrlStr << "removed successfully from store";
  else
    logInfo << fileUrlStr << "removal from store wasn't fully successful";

  return {status_l, status_h};
}