#include <QCryptographicHash>
#include <QSet>
#include <QStorageInfo>

#include "internal-helpers.hpp"

//
// Compares two errno error values in terms of pre-defined severity grades.
// Returns the severest of the two, or the first argument if both are equal.
//
int compErrnoSeverity(int error1, int error2) {
  const int e1            = error1;
  const int e2            = error2;
  const int worstSeverity = errnoSeverity(e1);

  if (const int sev = errnoSeverity(e2); sev > worstSeverity)
    return e2;

  return e1;
}

//
// Checks dirPath for existence, symbolic linkage and appropriate permissions.
// Returns errno on failure.
//
int dirHealth(const QString &dirPath) {

  const QFileInfo dir(dirPath);

  // Non-existence check
  if (!dir.exists()) {
    logWarning << dirPath << "doesn't exist yet";

    return EAGAIN;
  }

  // dirPath being a symlink is not allowed
  if (dir.isSymLink() || !dir.isDir()) {
    logCritical << dirPath << "is not a directory";

    return ECANCELED;
  }

  // Read, Write & Execute permissions are required
  if (!dir.isReadable() || !dir.isWritable() || !dir.isExecutable()) {
    logWarning << dirPath << "has inappropriate permissions";

    return EPERM;
  }

  return 0;
}

//
// Check for available free space in `dir` for accomodating `file`. Returns 0 on
// success, -1 if unable to check `dir` and ENOSPC if doesn't pass the check.
//
int diskFreeCheck(const QFileInfo &file, const QDir &dir) {

  const qint64 fileSize = file.size();
  const qint64 fsFree   = QStorageInfo(dir).bytesAvailable();

  // `bytesAvailable()` needs to be guarded first.
  if (fsFree == -1) {
    logWarning << "Cannot retrieve file system free space, copy may fail";
    return -1;
  }

  if (const qint64 s = fsFree - fileSize; s <= 0) {
    // Exact fit with 0 byte-margin is highly suspicious, and often prone to
    // failure
    logCritical << "Not enough free space available";
    logInfo << fileSize << "required but only" << fsFree << "is available";

    return ENOSPC;
  }

  return 0;
}

//
// Maps common errno to a severity rank for comparison purposes. Higher = more
// severe. Unlisted/unexpected errno (e.g. raw ec.value() from std::error_code)
// is treated as less severe than ECANCELED.
//
int errnoSeverity(int e) {
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
  case EPERM:
    return 4;
  case EIO:
    return 5;
  case ECANCELED:
    return 7;
  default:
    return 6;
  }
}

//
// File digest (SHA256) computer. Returns unseparated 64-char string for the
// computed digest, empty otherwise.
//
QString hashFile(const QString &path) {

  QFile file(path);

  if (!file.open(QIODeviceBase::ReadOnly))
    return {};

  QCryptographicHash hash(QCryptographicHash::Sha256);

  // Read in chunks; readAll() crashes
  while (!file.atEnd())
    if (const QByteArray &chunk = file.read(ioChunkSize); !chunk.isEmpty())
      hash.addData(chunk);

  return hash.result().toHex();
}

//
// Function to compute file digest and stage file for commit simultaneously.
// Utilizes QSaveFile class since it is a safer alternative than QFile::write().
// Takes an *opened* QFile handler as source and atomically copies to an
// *opened* QSaveFile handler destination. Returns a `hash_and_stage_t` struct.
// `status` is errno:{0, EIO}, `hash` is the unseparated 64-char string for the
// computed digest or empty, and `stagedPath` is the abosolute path string to
// the staged file pending commit or empty. `hash` and `stagedPath` both are
// returned empty on error.
//

hash_and_stage_t hashAndStageFile(QFile &srcFile, QSaveFile &stageFile) {

  QCryptographicHash hash(QCryptographicHash::Sha256);

  // Read in chunks; readAll() crashes
  while (!srcFile.atEnd()) {
    const QByteArray &buffer = srcFile.read(ioChunkSize);

    if (buffer.isEmpty()) {
      logCritical << "Read failed" << srcFile.errorString();
      stageFile.cancelWriting();

      return {EIO, {}, {}};
    }

    hash.addData(buffer);

    if (stageFile.write(buffer) != buffer.size()) {
      logCritical << "Write failed" << stageFile.errorString();
      stageFile.cancelWriting();

      return {EIO, {}, {}};
    }
  }

  return {0, hash.result().toHex(), stageFile.fileName()};
}

//
// Validate if `ch` is a hex character. No Qt-native methods exists as of now.
// Returns true if `ch` is a hex character, false otherwise.
//
bool isHexDigit(QChar ch) noexcept {

  const char c = ch.toLatin1();

  return (c >= '0'_L1 && c <= '9'_L1) || (c >= 'a'_L1 && c <= 'f'_L1) ||
         (c >= 'A'_L1 && c <= 'F'_L1);
}

//
// Isolates the first set `FileMan::Category` enum from possible combination of
// `FileMan::Categories` flag. Returns `FileMan::None` if nothing matched.
//
FileMan::Category lowestSetFlag(const FileMan::Categories categories) {
  const auto v = static_cast<unsigned>(categories);

  if (v == 0)
    return FileMan::None;

  return static_cast<FileMan::Category>(v & (~v + 1U));
}

//
// Returns the lowest available rename suffixed name string for `fileName` in
// `location`. Empty if exceeds 1023 attempts.
//
QString renameFile(const QString &fileName, const QDir &location) {

  // Trivial case: fileName is already unique, no renames necessary
  if (location.exists(fileName))
    return fileName;

  // Find the first empty rename suffix; e.g. existing-name_2.ext
  // having even anything near 1000 rename-suffixed same files is atypical
  const QFileInfo  file(fileName);
  const QString   &baseName = file.completeBaseName();
  const QString   &suffix   = '.'_L1 + file.suffix();
  constexpr qint64 lim      = 1024;

  // Using fast & efficient O(1) QSet for lookup rather than expending IO
  // QDir::exists()
  const QStringList   contents = location.entryList();
  const QSet<QString> contentsSet(contents.begin(), contents.end());

  for (int i = 1; i < lim; ++i) {
    const QString &candid = baseName + '_'_L1 + QString::number(i) + suffix;
    if (!contentsSet.contains(candid))
      return candid;
  }

  logWarning << "File count limit exceeded";

  return {};
}

//
// Sets read, write and execute permissions on a directory at path. Iterates
// from strictest scope (owner level) to broadest (world level). Returns true on
// success, false if setting permission failed on all levels. Detailed errors
// are stored in ec std::error_code object.
//
bool setDirPerms(const std::filesystem::path &dirPath, std::error_code &ec) {

  using std::filesystem::perm_options;
  using std::filesystem::permissions;
  using std::filesystem::perms;

  const QList<perms> permScope = {perms::owner_all, perms::group_all, perms::others_all};

  for (const auto &scope : permScope) {
    permissions(dirPath, scope, perm_options::add, ec);

    if (ec.value() == 0)
      return true;
  }

  return false;
}

//
// File validation routines. Returns 0 if successful, errno:{EINVAL, ENOENT}
// otherwise.
//
int validateFile(const QFileInfo &file) {

  //
  // Check existence & file type; only take regular files - from wherever
  // accessible (including from /sys as long as permission allows but we
  // handle these unacceptable files next)
  //
  if (const bool exists = file.exists(); !exists || !file.isFile())
    return exists ? EINVAL : ENOENT;

  // regular, but unusual files; e.g. textstreams or files under /proc
  if (file.size() <= 0)
    return EINVAL;

  return 0;
}

//
// Overload of `validateFile(const QFileInfo &)`. Additionally checks for file
// size constraints. Returns 0 if successful, errno:{EINVAL, ENOENT, EFBIG}
// otherwise.
//
int validateFile(const QFileInfo &file, const qint64 fileSize) {

  if (const int e = validateFile(file); e != 0)
    return e;

  if (file.size() > fileSize) {
    logCritical << "File size exceeding" << QLocale().formattedDataSize(fileSize)
                << "is not supported";
    return EFBIG;
  }

  return 0;
}