#include <QCoreApplication>
#include <QMimeDatabase>
#include <QNtfsPermissionCheckGuard>
#include <QStandardPaths>

#include "fileman_p.hpp"
#include "internal-helpers.hpp"
#include <fileman>

Q_LOGGING_CATEGORY(fileman, "FileMan")

// [Private] FileMan is meant to be a singleton
FileMan *FileMan::s_singletonInstance = nullptr;

//
// The QML_SINGLETON factory referer. Qt/QML calls this itself when QML
// code touches FileMan.foo. Required as long as QML_SINGLETON is on the class.
//
FileMan *FileMan::create(QQmlEngine * /*unused*/, QJSEngine * /*unused*/) {

  if (FileMan::s_singletonInstance == nullptr)
    qFatal("FileMan: no instance constructed. FileMan must be explicitly "
           "instantiated before the QML engine loads");

  // The singleton's lifetime is owned by this TU (main()/first construction),
  // not by the QQmlEngine. Make that explicit so the engine never attempts
  // to garbage-collect or delete it.
  QQmlEngine::setObjectOwnership(FileMan::s_singletonInstance, QQmlEngine::CppOwnership);

  return FileMan::s_singletonInstance;
}

//
// Wrap the instance pointer in an accessor rather than exposing
// plainly. Returns a (maybe null) reference to the instance. Can be used to
// refer to an externally constructed, but not referenced FileMan object.
//
FileMan &FileMan::instance() {
  if (FileMan::s_singletonInstance == nullptr)
    qFatal("FileMan: no instance constructed. FileMan must be explicitly instantiated before "
           "invoking the instance");

  return *s_singletonInstance;
}

// Constructor/Initializer
FileMan::FileMan(QObject *parent) : QObject(parent), d(std::make_unique<Private>()) {

  Q_ASSERT(QCoreApplication::instance());

  if (s_singletonInstance != nullptr)
    qFatal("FileMan: A second FileMan instance was constructed; only one "
           "instance may exist per process");

  if (QCoreApplication::applicationName().isEmpty())
    qFatal("FileMan: QCoreApplication::applicationName() is empty. "
           "FileMan cannot safely isolate its data store. Aborting");

  this->d->dataDir      = resolveDataDir();
  this->d->hashDir      = createPathComp(this->d->dataDir, "common"_L1);
  this->d->categoryDirs = Private::createCategoryDirList(this->d->dataDir);
  this->d->categoryList = Private::createCategoryList(this->d->dataDir);
  this->d->storeLock =
      std::make_unique<QLockFile>(QDir(this->d->dataDir).absoluteFilePath(".fileman.lock"_L1));

  FileMan::s_singletonInstance = this;
}

// Destructor, clears the static pointer
FileMan::~FileMan() {

  if (FileMan::s_singletonInstance == this)
    FileMan::s_singletonInstance = nullptr;
}

//
// Opaque/PIMPL structs add_file_t and rm_file_t for maintaining ABI
// compatiblity
//

// Empty constructors
add_file_t::add_file_t() : d(new AddFileType) {}
rm_file_t::rm_file_t() : d(new RmFileType) {}

// Constructors having references to members
add_file_t::add_file_t(int status, const QUrl &location) : d(new AddFileType) {
  this->d->status   = status;
  this->d->location = location;
}

rm_file_t::rm_file_t(int status_l, int status_h) : d(new RmFileType) {
  this->d->status_l = status_l;
  this->d->status_h = status_h;
}

// Copy/Move constructors
add_file_t::add_file_t(const add_file_t &)                = default;
add_file_t &add_file_t::operator=(const add_file_t &)     = default;
add_file_t::add_file_t(add_file_t &&) noexcept            = default;
add_file_t &add_file_t::operator=(add_file_t &&) noexcept = default;

rm_file_t::rm_file_t(const rm_file_t &)                = default;
rm_file_t &rm_file_t::operator=(const rm_file_t &)     = default;
rm_file_t::rm_file_t(rm_file_t &&) noexcept            = default;
rm_file_t &rm_file_t::operator=(rm_file_t &&) noexcept = default;

// Destructors
add_file_t::~add_file_t() = default;
rm_file_t::~rm_file_t()   = default;

// Accesors
int  add_file_t::status() const { return this->d ? this->d->status : -1; }
QUrl add_file_t::location() const { return this->d ? this->d->location : QUrl(); }
// True only after this object has been moved-from. A default-constructed add_file_t is never null.
bool add_file_t::isNull() const noexcept { return !this->d; }

int  rm_file_t::status_h() const { return this->d ? this->d->status_h : -1; }
int  rm_file_t::status_l() const { return this->d ? this->d->status_l : -1; }
bool rm_file_t::isNull() const noexcept { return !this->d; }

////////////////////////////////////////////////////////////////////////////////
//  Private definitions
////////////////////////////////////////////////////////////////////////////////

//
// Acquires explicit thread lock & lock on the store for serialized
// access and preventing race conditions. Returns 0 on success, errno on
// failure.
//
int FileMan::acquireStoreLock() {

  constexpr qint32 lockLifetime = 40000; // 40 sec
  constexpr qint32 waitDuration = 5000;  // 5 sec

  this->d->storeMutex.lock();

  this->d->storeLock->setStaleLockTime(lockLifetime);

  if (!this->d->storeLock->tryLock(waitDuration)) {
    int e = EIO;

    switch (this->d->storeLock->error()) {
    case QLockFile::LockFailedError:
      logCritical << "Store busy, could not acquire lock";
      e = EBUSY;
      break;
    case QLockFile::PermissionError:
      logCritical << "Permission error in store, could not acquire lock";
      e = EACCES;
      break;
    default:
      logCritical << "IO error in store, could not acquire lock";
      break;
    }

    this->d->storeMutex.unlock();

    return e;
  }

  return 0;
}

//
// Clears all entries in the dictionaries.
//
void FileMan::clearAllEntries() {
  const QWriteLocker locker(&this->d->dictLock);

  this->d->hashToLinkDict.clear();
  this->d->linkToHashDict.clear();
}

//
// Concatenates two path components and returns the combined
// clean path with platform relevant directory separator. Never returns empty
// string.
//
QString FileMan::createPathComp(const QString &comp1, const QString &comp2) {
  return QDir::cleanPath(QDir::toNativeSeparators(comp1 + '/'_L1 + comp2));
}

//
// Dumps all entries in both the dictionaries.
//
void FileMan::dumpAllEntries() {
  const QReadLocker locker(&this->d->dictLock);

  logDebug << "hashToLinkDict:";
  for (const auto &[key, value] : std::as_const(this->d->hashToLinkDict).asKeyValueRange())
    logDebug << qPrintable(key) << ": " << value;

  logDebug << "linkToHashDict:";
  for (const auto &[key, value] : std::as_const(this->d->linkToHashDict).asKeyValueRange())
    logDebug << qPrintable(key) << ": " << value;
}

//
// Attempts appropriate repairs on `dir` following the provided `error`. Returns
// true if repairs were successful, false otherwise.
//
bool FileMan::fixDirHealth(const QString &dir, int error, FileMan::Category category) {

  if (error != 0 && error != EAGAIN && error != ECANCELED && error != EPERM) {
    logCritical << "Critical error on store, can't continue";
    logDebug << "Error branch [" << dir << "]:" << error;

    return false;
  }

#ifdef Q_OS_WIN
  // Enables permission operations on NTFS on Windows within this scope
  QNtfsPermissionCheckGuard permissionGuard;
#endif

  if (error == EPERM) {
    // Attempt to fix permissions
    const std::filesystem::path dir_stl = dir.toStdString();
    std::error_code             ec;

    if (!setDirPerms(dir_stl, ec)) {
      logCritical << "Couldn't set appropriate permissions on" << dir;
      logDebug << "Error branch [" << dir << "]:" << error;
      logDebug << "Error code: " << ec.value();

      return false;
    }

    return true;
  }

  if (error == ECANCELED) {
    // QDir(dir).remove() would require extracting parent dir and leaf dir
    // names. QFile::remove() is simpler
    if (!QFile::remove(dir)) {
      logCritical << "Critical error on store, can't continue";
      logDebug << "Error branch [" << dir << "]:" << error;

      return false;
    }

    error = EAGAIN;
  }

  if (error == EAGAIN) {

    const bool dirCreated =
        (category == FileMan::None) ? this->mkDataDir() : this->mkDataDir(category);

    if (!dirCreated) { // Could be other advanced permission errors
                       // (e.g.
                       // ACL)
      logCritical << "Critical error on store, please check" << dir
                  << "and its parent dirs for permission issues";
      logDebug << "Error status [" << dir << "]:" << error;

      return false;
    }

    // Redundant, but future-proof
    return true;
  }

  return true;
}

//
// Checks whether dictionary is empty or not. Returns true if it is,
// false if not
//
bool FileMan::isDictEmpty(const QHash<QString, QString> &dict) {
  const QReadLocker locker(&this->d->dictLock);

  return dict.isEmpty();
}

//
// Looks-up the hash entry for `lnkEntry` in the in-memory hash-table.
// Returns the entry string. Empty if entry doesn't exists.
//
QString FileMan::lookupHashByLink(const QString &lnkEntry) {

  const QReadLocker locker(&this->d->dictLock);

  return this->d->linkToHashDict.value(lnkEntry, {});
}

//
// Looks-up the symlink entry for `hash` in the in-memory hash-table.
// Returns the entry string. Empty if entry doesn't exists.
//
QString FileMan::lookupLinkByHash(const QString &hash) {

  const QReadLocker locker(&this->d->dictLock);

  return this->d->hashToLinkDict.value(hash, {});
}

//
// Creates the full path to dataDir/common as required. Returns true
// if path successfully created or exists, false otherwise.
//
bool FileMan::mkDataDir() {

  if (!QDir().mkpath(this->d->hashDir)) {
    logCritical << "Failed to create" << this->d->hashDir;

    return false;
  }

  return true;
}

//
// Overload of `mkDataDir()`. Creates the full path to
// dataDir/<category> as required. Returns true if path successfully created or
// exists, false otherwise.
//
bool FileMan::mkDataDir(FileMan::Category category) {

  const QString &categoryDirPath(this->d->categoryList.value(category).location);

  if (!QDir().mkpath(categoryDirPath)) {
    logCritical << "Failed to create" << categoryDirPath;

    return false;
  }

  return true;
}

//
// Validator function before adding `file` to store. Returns a
// `pre_add_validate_t` struct containing {int, FileMan::Categories} members.
// Returns {0, FileMan::<valid_categories>} if successful, {errno,
// FileMan::None} if otherwise. `errno` maybe one of `{EFBIG, EINVAL, ELOOP,
// EOPNOTSUPP, ENOENT}`.
//
FileMan::pre_add_validate_t FileMan::preAddValidate(const QFileInfo &file) {

  if (file.isSymLink()) {

    //
    // A fuzzy function to test symlink recursion
    //
    // Someone could be joking or could actually be their OPSEC, but we
    // shouldn't allow it too far. Note, this is also handled by the
    // filesystem natively but just in case
    //
    QString p = QFileInfo(file.symLinkTarget()).absoluteFilePath();

    qint8 allowedLimit = 41;

    while (allowedLimit-- > 0) {
      const QFileInfo t(p);
      if (!t.isSymLink())
        break;
      p = QFileInfo(t.symLinkTarget()).absoluteFilePath();
    }

    if (allowedLimit < 0)
      return {ELOOP, FileMan::None};
  }

  // A 5 mins long 96 kHz 24-bit audio file could easily reach this ceiling
  // images are much more forgiving though
  constexpr qint32 gibVal = 1073741824;

  if (const int e = validateFile(file, gibVal); e != 0)
    return {e, FileMan::None};

  //
  // only take files having <category>/* mime types, e.g. audio/ogg,
  // audio/flac, image/svg+xml, image/png, video/mp4 etc
  // this rules out text files common under unacceptable paths like /sys
  //
  const QMimeDatabase mimeDb;
  const QString      &mime =
      mimeDb.mimeTypeForFile(file.canonicalFilePath(), QMimeDatabase::MatchContent)
          .name()
          .toLower();

  logDebug << "MIME:" << mime;

  if (mime.isEmpty()) {
    logCritical << "MIME type could not be determined";

    return {EOPNOTSUPP, FileMan::None};
  }

  FileMan::Categories categories = FileMan::None;

  for (const auto &[cat, catInfo] : this->d->categoryList.asKeyValueRange())
    if (mime.startsWith(catInfo.name + '/'_L1) ||
        (catInfo.name == "video"_L1 && mime.contains("/x-matroska"_L1)))
      categories |= cat;

  if (categories == FileMan::None) {
    logCritical << "File type not supported for this operation";
    return {EOPNOTSUPP, FileMan::None};
  }

  return {0, categories};
}

//
// Validates the provided absolute path of file `lnkPath`. Returns a
// `pre_rm_validate_t` struct `{int status, QString lnkEntry, QString
// hashEntry}`. Takes an optional `enforcedCheck` bool for stricter validation.
// On validation passed, `status` is 0, `lnkEntry` contains the constructed
// entry for `lnkPath` and `hashEntry` may either contain the name of the hash
// file in store or empty if `lnkPath` is resolved to be dangling. Orphaned hash
// files are never returned. On error, `status` is errno:{EAGAIN, ECANCELED,
// EINVAL, ENOENT}, `lnkEntry` may be empty but `hash` is always empty.
//
FileMan::pre_rm_validate_t FileMan::preRmValidate(const QString &lnkPath,
                                                  const bool     enforcedCheck) {

  const QFileInfo lnkFile(lnkPath);
  const QString  &lnkFileName = lnkFile.fileName();
  const QDir     &lnkDir      = lnkFile.dir();
  const QString  &lnkDirName  = lnkDir.dirName(); // i.e. parent dir name
  const QString  &lnkDirPath  = QDir::cleanPath(lnkDir.absolutePath());
  const QString  &lnkEntry    = FileMan::createPathComp(lnkDirName, lnkFileName);

  // `lnkPath` could be a directory path or TLD /, C: etc
  if (lnkFileName.isEmpty() || lnkDirName.isEmpty()) {
    logCritical << lnkPath << "is an invalid path. Refusing to proceed.";

    return {EINVAL, {}, {}};
  }

  const QString categoryDirPath = this->d->categoryDirs.value(lnkDirName, {});
  const QDir    categoryDir(categoryDirPath);

  if (categoryDirPath.isEmpty()) {
    logCritical << lnkPath << "is not a valid file in store";

    return {EINVAL, {}, {}};
  }

  if (lnkDirPath != categoryDirPath) {
    logCritical << lnkPath << "is not a path to store";

    return {EINVAL, {}, {}};
  }

  // Files under dataDir/<category> are usually symlinks
  if (!lnkFile.isSymLink() && !lnkFile.exists()) {
    // This file actually doesn't exist
    logWarning << lnkPath << "doesn't exist in store";

    return {ENOENT, {}, {}};
  }

  // See the target of the symlink first but don't resolve yet
  // QDir::filePath() cleverly returns the target path unchanged if it's
  // absolute, lexically manipulates the string if it's relative
  const QDir      hashDir(this->d->hashDir);
  const QString   lnkTargetPath = QDir::cleanPath(categoryDir.filePath(lnkFile.symLinkTarget()));
  const QFileInfo lnkTarget(lnkTargetPath);
  const QString   lnkTargetFileName = lnkTarget.fileName();

  logDebug << "Symlink target:" << lnkTargetPath;

  // All healthy symlinks canonically point to `hashDir`
  if (lnkTarget.absolutePath() != hashDir.absolutePath()) {
    // An alien symlink in the store, disallow symlink resolution
    logWarning << lnkPath << "points to outside the store or not a symbolic link";

    return {0, lnkEntry, {}};
  }

  // Target path may still be a crafted jump-symlink residing in the store
  if (lnkTarget.isSymLink()) {
    logWarning << lnkTargetPath << "is unexpectedly a symlink";

    return {0, lnkEntry, {}};
  }

  // Target may not exist
  if (!lnkTarget.exists()) {
    logWarning << lnkTargetPath << "doesn't exist on store";

    return {0, lnkEntry, {}};
  }

  //
  // reaching here means provided symlink & target are both valid & safe
  // and not dangling. Proceed to check the hashFile next
  //

  // Target may not pass our expected validations, `preAddValidate()` handles
  // some of this already, but do only when called for an enforced check since
  // this is a costly function and redos some jobs already done. Use the lighter
  // `validateFile()` otherwise

  int e = 0;
  e     = (enforcedCheck) ? this->preAddValidate(lnkTarget).status : validateFile(lnkTarget);

  if (e != 0) {
    logWarning << lnkTargetPath << "didn't pass validations";
    logDebug << "Error:" << e;

    return {0, lnkEntry, {}};
  }

  // Valid files ought to have 64 characters exactly (sha256 digest length)
  constexpr qint8 nameLength = 64;
  if (const qint64 s = lnkTargetFileName.size(); s != nameLength) {
    logWarning << lnkTargetPath << "is not a valid file in store (wrong name)";
    logDebug << "Base name size:" << s << "vs" << nameLength;

    return {0, lnkEntry, {}};
  }

  // They also have exactly hex characters: [a-fA-F] & [0-9]
  for (const QChar &c : lnkTargetFileName)
    if (!isHexDigit(c)) {
      logWarning << lnkTargetPath << "is not a valid file in store (wrong name)";
      logDebug << "Base name" << lnkTargetFileName << "doesn't look like a sha256 hash";

      return {0, lnkEntry, {}};
    }

  // Validate the digest against file name. This is a costly operation so
  // saving this for last and only when called for enforced check
  if (enforcedCheck)
    if (const QString h = hashFile(lnkTargetPath); h != lnkTargetFileName) {
      logWarning << "Digest mismatch for" << lnkTargetPath;

      return {0, lnkEntry, {}};
    }

  return {0, lnkEntry, lnkTargetFileName};
}

//
// Registers the entries to the in-memory hash-tables.
//
void FileMan::registerEntry(const QString &lnkEntry, const QString &hash) {

  const QWriteLocker locker(&this->d->dictLock);

  this->d->hashToLinkDict.insert(hash, lnkEntry);
  this->d->linkToHashDict.insert(lnkEntry, hash);
}

//
// Removes the entries from the in-memory hash-tables.
//
void FileMan::removeEntry(const QString &lnkEntry, const QString &hash) {

  const QWriteLocker locker(&this->d->dictLock);

  this->d->hashToLinkDict.remove(hash);
  this->d->linkToHashDict.remove(lnkEntry);
}

//
// Releases the explicit thread lock & lock on the store.
//
void FileMan::releaseStoreLock() {

  this->d->storeLock->unlock();
  this->d->storeMutex.unlock();
}

//
// Returns the absolute path to app's data dir as string; never returns null.
// Note this function doesn't guarranty existence.
//
QString FileMan::resolveDataDir() {
  const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

  // Edge-case when `applicationName()` may not be set
  Q_ASSERT_X(!appDataDir.isEmpty(), "FileMan::getDataDir",
             "AppDataLocation resolved empty - was applicationName() set "
             "before first call?");

  return appDataDir;
}

//
// Function to reverse lookup an absolute-path `target` from symlinks in
// `searchDir`. Returns a `rev_lookup_symlink_t` struct having members {status,
// location}. `location` is a string to either absolute path of the symlink or
// empty if none is found or faced other issues. `status` is errno:{0, EPERM}.
//
FileMan::rev_lookup_symlink_t FileMan::revLookupSymlink(const QDir    &searchDir,
                                                        const QString &targetPath) {

#ifdef Q_OS_WIN
  // Enables permission lookup on NTFS on Windows within this scope
  QNtfsPermissionCheckGuard permissionGuard;
#endif

  // Just for logging purposes, should show the original format it was
  // constructed with
  const QString &searchDirPath = searchDir.path();

  // Searching files and looking up symlink targets only need the executable
  // permission
  if (const QFileInfo f(searchDir, "."_L1); !f.isExecutable())
    return {EPERM, {}};

  // Let's assume `targetPath` is hashDir/hash and try some shortcuts
  const QString targetName  = QFileInfo(targetPath).fileName();
  const QString lnkEntry    = lookupLinkByHash(targetName);
  const bool    entryExists = !lnkEntry.isEmpty();
  const bool    isManaged   = this->d->storeManaged;

  if (isManaged && entryExists) {

    const QFileInfo lnkFile(QDir(this->d->dataDir).filePath(lnkEntry));
    const QString   lnkPath = lnkFile.absoluteFilePath();
    logDebug << "targetPath:" << targetPath;
    // Implies existence _and_ symlink type (dangling or not)
    const bool lnkExists = lnkFile.isSymLink();

    if (lnkExists) {
      const QString lnkTarget =
          QDir::cleanPath(searchDir.absoluteFilePath(lnkFile.symLinkTarget()));
      logDebug << "searchDir:" << searchDirPath;
      logDebug << "lnkTarget:" << lnkFile.symLinkTarget();
      logDebug << "lnkTarget (cleaned):" << lnkTarget;

      if (lnkTarget == QDir::cleanPath(targetPath))
        return {0, QDir::cleanPath(lnkFile.absoluteFilePath())};
    }

    // Either symlink doesn't match target in reality or it doesn't exist on
    // fs. Either way, remove stale entry from the dictionaries, and proceed
    // to full dir check

    logDebug << "Removing stale" << lnkEntry << "from caches.";
    this->removeEntry(lnkEntry, targetName);
  }

  const QList<QFileInfo> &dirContents =
      searchDir.entryInfoList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot | QDir::System);

  if (dirContents.isEmpty()) {

    // Nothing present to lookup anyway
    logDebug << searchDirPath << "is empty.";

    return {EAGAIN, {}};
  }

  // Search for symlinks and compare the resolved target with `target`
  for (const QFileInfo &file : dirContents) {

    if (!file.isSymLink())
      continue;

    const QString &resolvedTarget =
        QDir::cleanPath(searchDir.absoluteFilePath(file.symLinkTarget()));

    if (resolvedTarget == QDir::cleanPath(targetPath)) {

      if (isManaged) {
        const QString lnkEntry = FileMan::createPathComp(file.dir().dirName(), file.fileName());

        logDebug << "Updating hash-tables with missing [" << lnkEntry << "," << targetName
                 << "] entry";

        this->registerEntry(lnkEntry, targetName);
      }

      return {0, QDir::cleanPath(file.absoluteFilePath())};
    }
  }

  // Reaching here means no symlinks match `target`

  if (!isManaged && entryExists) // Stale entry in dictionaries
    this->removeEntry(lnkEntry, targetName);

  return {ENOENT, {}};
}

//
// Shrinks the dictionary sizes to their contents size
//
void FileMan::squeezeDicts() {
  QWriteLocker locker(&this->d->dictLock);

  this->d->hashToLinkDict.squeeze();
  this->d->linkToHashDict.squeeze();
}

//
// Validates and attempts repairs on dataDir, hashDir and categoryDir(s).
// Returns validate_store_t struct having {int d_status, int h_status, int
// c_status}. Values are 0 on success, errno on failure.
//
FileMan::validate_store_t FileMan::validateStore() {

  FileMan::validate_store_t status = {-1, -1, {}};

  for (const QString &dir : {this->d->dataDir, this->d->hashDir}) {

    const int e         = dirHealth(dir);
    int      &dirStatus = (dir == this->d->dataDir) ? status.d_status : status.h_status;

    logDebug << dir << "directory health:" << e;

    if (!fixDirHealth(dir, e)) {
      dirStatus = e;

      // The checks are serialized based on hierarchy so any errors are fatal,
      // blocking further continuation
      // logDebug << "Error status [d, h, c]:" << status.d_status
      //  << status.h_status << status.c_status;

      return status;
    }

    dirStatus = 0;
  }

  for (const auto &[cat, catInfo] : this->d->categoryList.asKeyValueRange()) {

    logDebug << "Store path (categorized):" << catInfo.location;

    const int e = dirHealth(catInfo.location);

    logDebug << catInfo.name << "directory health:" << e;

    if (!fixDirHealth(catInfo.location, e, cat)) {
      status.c_status.insert(cat, e);

      continue;
    }

    status.c_status.insert(cat, 0);
  }

  // logDebug << "Error status [d, h, c]:" << status.d_status << status.h_status
  //  << status.c_status;

  return status;
}