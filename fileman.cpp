#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QQmlEngine>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>
#include <QVariant>
#include <cerrno>
#include <cstring>

#include <fileman.hpp>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

FileMan::FileMan(QObject *parent) : QObject(parent) {}

static const QList categoryList = { FileMan::Audio, FileMan::Image, FileMan::Video };
static const qsizetype catListSize = categoryList.size();

FileMan *FileMan::s_singletonInstance = nullptr;

// Translate enums (i.e. integers) to strings, string is never empty
constexpr static QLatin1StringView catStr(FileMan::Categories category) noexcept
{
    switch (category) {
        case FileMan::Audio: return QLatin1StringView("audio");
        case FileMan::Image: return QLatin1StringView("image");
        case FileMan::Video: return QLatin1StringView("video");
    }
    Q_UNREACHABLE();
}

// Show the app's data dir; never returns null
QString FileMan::getDataDir()
{ return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation); }

static QString mkDataDir()
{
    const QDir dataDir(FileMan::getDataDir());

    // dataDir/common is the place for all raw blobs
    if (!dataDir.mkpath("common")) {
        qCritical().noquote() << "FileMan:\tFailed to create" << dataDir.filePath("common");
        return {};
    }
    return dataDir.absoluteFilePath("common");
}

// Overload function, takes the enum as argument e.g. mkDataDir(FileMan::Audio)
static QString mkDataDir(FileMan::Categories category)
{
    const QDir dataDir(FileMan::getDataDir());
    const QString categoryStr = catStr(category);
    if (!dataDir.mkpath(categoryStr)) {
        qCritical().noquote() << "FileMan:\tFailed to create" << dataDir.filePath(categoryStr);
        return {};
    }
    return dataDir.absoluteFilePath(categoryStr);
}

// Validator function before adding to store
static int preAddValidate(const QString &path, FileMan::Categories category)
{
    QFileInfo file(path);

    if (file.isSymLink()) {
        //
        // A fuzzy function to test symlink recursion
        //
        // Someone could be joking or could actually be their OPSEC, but we
        // shouldn't allow it too far. Note, this is also handled by the
        // filesystem natively but just in case
        //
        QString p = QFileInfo(file.symLinkTarget()).absoluteFilePath();
        int i = 0;
        constexpr int loopDepth = 41;
        for (; i < loopDepth; i++) {
            const QFileInfo t(p);
            if (!t.isSymLink()) break;
            p = QFileInfo(t.symLinkTarget()).absoluteFilePath();
        }
        if (i >= 41) return ELOOP;
    }

    // Check existence & file type; only take regular files - from wherever
    // accessible (including from /sys as long as permission allows but we
    // handle these unacceptable files next)
    if (const bool exists = QFileInfo::exists(path); !exists || !file.isFile())
        return exists ? EINVAL : ENOENT;

    // regular, but unusual files; e.g. textstreams or files under /proc
    if (file.size() <= 0) return EINVAL;

    // A 5 mins long 96 kHz 24-bit audio file could easily reach this ceiling
    // images are much more forgiving though
    constexpr int gibVal = 1073741824;
    if (file.size() > gibVal) {
        qCritical().noquote() << "FileMan:\tFile size exceeding 1GB not supported";
        return EFBIG;
    }

    const QMimeDatabase mimeDb;
    const QString mime = mimeDb
        .mimeTypeForFile(file.canonicalFilePath(), QMimeDatabase::MatchContent)
        .name().toLower();
    qDebug().noquote() << "FileMan:\tMIME:" << mime;
    // only take files having <category>/* mime types, e.g. audio/ogg,
    // audio/flac, image/svg+xml, image/png, video/mp4 etc
    // this rules out text files common under unacceptable paths like /sys
    if (const QLatin1StringView c = catStr(category); mime.isEmpty() ||
        !mime.startsWith(c + u"/")) {
        // .mkv appears as application/x-matroska instead of video/x-matroska
        if (c == "video" && mime.contains("/x-matroska")) return 0;
        qCritical().noquote() << "FileMan:\tFile type not supported for this operation";
        return EOPNOTSUPP;
    }

    return 0;
}

// File digest (SHA256) computer
static QString sha256File(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODeviceBase::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    // Read in chunks; readAll() crashes
    // 64KiB is a good chunk size for almost all purposes including network
    // files, although 1MiB would've been more ideal for local disk files
    constexpr qint64 chunkSize = 65536;
    while (!file.atEnd()) {
        if (const auto chunk = file.read(chunkSize); !chunk.isEmpty())
            hash.addData(chunk);
    }
    return hash.result().toHex();
}

// This is only for invoking from qml
QString FileMan::getFileName(const QUrl &fileUrl) { return fileUrl.fileName(); }

// A separate function for a small job, but keeps the clutter isolated
static QString renameFile(const QString &fileName, const QDir &location)
{
    // Find the first empty rename suffix; e.g. existing-name_2.ext
    // having even anything near 1000 rename-suffixed same files is atypical
    const QString baseName = QFileInfo(fileName).completeBaseName();
    const QString suffix = QLatin1StringView(".")
                            + QFileInfo(fileName).suffix();
    constexpr qint64 lim = 1024;
    for (int i = 1; i < lim; ++i) {
        const QString candid = baseName
                                + QLatin1StringView("_")
                                + QString::number(i) + suffix;
        if (!location.exists(candid)) return candid;
    }
    qWarning().noquote() << "FileMan:\tFile count limit exceeded"; return {};
}

//
// Take URL schemes as input, since this is common output from both file dialogs
// and qml. Use regular paths internally
//
// Outputs K-V pairs; {{ "error": <int> }, { "path": <url> }}
// We use .toInt() and .toString() to extract the V[alues] from K[eys]
//
QVariantMap FileMan::addFile(const QUrl &fileUrl, FileMan::Categories category)
{
    //
    // URL  =>  Translated path
    //
    // file:thisFileOnCurrentDir.txt    =>  ./thisFileOnCurrentDir.txt
    // file:///path/to/file =>  /path/to/file
    // servername:///path/to/file   =>  //servername/path/to/file
    // ftp://servername.com/path/to/folder/ =>  ""
    // /path/to/file    =>  ""
    //
    const QString filePath = fileUrl.toLocalFile();
    const QString src = QFileInfo(filePath).canonicalFilePath();
    if (src.isEmpty()) {
        qCritical().noquote() << "FileMan:\tNo such file:" << filePath;
        return {{"error", ENOENT}, {"path", {}}};
    }
    qDebug().noquote() << "FileMan:\tResolved file:" << src;

    // File validation

    // Also find the matching category for the file
    int idx = -1;
    int err = 0;
    for (int i = 0; i < catListSize; ++i) {
        const auto c = categoryList.at(i);
        if ((category & c) == 0) continue;
        qDebug().noquote() << "FileMan:\tValidating for category" << catStr(c);
        if (err = preAddValidate(src, c); err == 0) { idx = i; break; }
    }
    if (idx < 0 || idx >= catListSize) {
        qCritical().noquote() << "FileMan:\tNo valid category found for" << src;
        return {{"error", err}, {"path", {}}};
    }
    const auto catSelected = categoryList.at(idx);
    qDebug().noquote() << "FileMan:\tSelected category:" << catStr(catSelected);

    // Hash the file to get unanimous identity
    const QString hash = sha256File(src);
    if (hash.isEmpty()) {
        qCritical().noquote() << "FileMan:\tHash computation failed";
        return {{"error", EIO}, {"path", {}}};
    }

    // Raw data files/blobs as dataDir/common/<hash>
    QString dest("");
    if (const QString d = mkDataDir(); d.isEmpty()) {
        return {{"error", ENOENT}, {"path", {}}};
    } else dest = QDir(d).filePath(hash);

    QDir lnkDir;
    if(const QString d = mkDataDir(catSelected); d.isEmpty())
        return {{"error", ENOENT}, {"path", {}}};
    else lnkDir.setPath(d);

    // Return the present symlink if adding same file with a different name
    if (QFile::exists(dest)) {
        qInfo().noquote() << "FileMan:\t" << src << "already available on store";

        const QList<QFileInfo> contents = lnkDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
        if (contents.isEmpty()) {
        // Either the store state is unreliable for now or permission issues
        // fail now to let user try again later
            qCritical().noquote() << "FileMan:\tNo known files found, store is inconsistent";
            return {{"error", ENOENT}, {"path", {}}};
        }

        // Search for symlinks and compare the target with `dest`
        for (const auto &f : std::as_const(contents)) {
            if (!f.isSymLink()) continue;
            const QString t = QDir::cleanPath(lnkDir.filePath(f.symLinkTarget()));
            if (t == dest)
                return {{"error", EEXIST}, {"path", QUrl::fromLocalFile(f.absoluteFilePath())}};
        }

        // If nothing matched, then store is surely unreliable (for now)
        qCritical().noquote() << "FileMan:\tStore is unreliable, please try again";
        return {{"error", EAGAIN}, {"path", {}}};
    }

    // Opportunistically try hardlinking the file, fallback to regular copying
    // if fails. Only UNIX for now
#ifdef Q_OS_UNIX
    bool hlStatus = false;
    {
        //
        // std::link(const char* source, const char* dest), returns -1 if
        // unsuccessful. The system-call error is assigned to "errno"
        //
        // Qt is utf16. toUtf8() translates that inline and constData()
        // provides the const char* type value, required by std::link()
        //
        if (::link(src.toUtf8().constData(), dest.toUtf8().constData()) == 0) {
            qDebug().noquote() << "FileMan:\tHardlinked" << src << "to" << dest;
            hlStatus = true;
        } else {
            const int e = errno;
            // EFAULT, EPERM & EXDEV are values for usual hardlinking errors
            if (e != EFAULT && e != EPERM && e != EXDEV) {
                qCritical().noquote() << "FileMan:\tProblem occured, can't operate";
                return {{"error", e}, {"path", {}}};
            }
            qWarning().noquote() << "FileMan:\tHardlinking failed, falling back to regular copy";
            qDebug().noquote() << "FileMan:\tError:" << e;
        }
    }


#elif defined(Q_OS_WIN)
    constexpr bool hlStatus = false;
    // Something with CreateLink or whatever
#endif
    if (!hlStatus) {
        QFile srcFile(src);
        if (!srcFile.open(QIODeviceBase::ReadOnly)) {
            qCritical().noquote() << "FileMan:\tCouldn't open" << src << "for reading";
            return {{"error", EPERM}, {"path", {}}};
        }

        const QString destDir = QFileInfo(dest).absoluteFilePath();

        // Check available (quota) free space before copying
        // bytesAvailable() needs to be guarded first
        if (qint64 s1 = QStorageInfo(destDir).bytesAvailable(); s1 == -1)
            qWarning().noquote() << "FileMan:\tCannot retrieve file system free space, copy may fail";
        else {
            if (qint64 free = s1 - srcFile.size(); free <= 0) {
                qCritical().noquote() << "FileMan:\tNot enough free space available";
                return {{"error", ENOSPC}, {"path", {}}};
            }
        }

        // QSaveFile is a safer alternative than QFile::write()
        QSaveFile destFile(dest);
        if (!destFile.open(QIODeviceBase::WriteOnly)) {
            qCritical().noquote() << "FileMan:\tCouldn't open" << dest << "for writing";
            return {{"error", EPERM}, {"path", {}}};
        }

        constexpr qint64 chunkSize = 65536;
        while (!srcFile.atEnd()) {
            const QByteArray buffer = srcFile.read(chunkSize);
            if (buffer.isEmpty()) break;
            if (destFile.write(buffer) != buffer.size()) {
                qCritical().noquote() << "FileMan:\tWrite failed";
                return {{"error", EIO}, {"path", {}}};
            }
        }
        if (!destFile.commit()) {
            qCritical().noquote() << "FileMan:\tCommit failed";
            return {{"error", EIO}, {"path", {}}};
        }
    }

    QString srcFileName = QFileInfo(src).fileName();
    QString savedPath = QDir::cleanPath(lnkDir.absoluteFilePath(srcFileName));
#ifdef Q_OS_WIN
    constexpr QLatin1StringView winLnkExt(".lnk");
    savedPath += winLnkExt;
#endif

    if (QFileInfo::exists(savedPath)) {
        // Rename the supplied file with rename-suffix, e.g. existing-file_2.ext
        // and org.munadi.Munadi.png_2.svg (weird naming to begin with)
        QString f = renameFile(srcFileName, lnkDir);
        qWarning().noquote() << "FileMan:\tSaving" << srcFileName << "as" << f << "since it already exists";
        srcFileName = f;
        savedPath = QDir::cleanPath(lnkDir.absoluteFilePath(srcFileName));
#ifdef Q_OS_WIN
        // Windows requires .lnk extension for a valid symlink
        savedPath += winLnkExt;
#endif
    }

    // Symlink the unanimous hashed file as the supplied file name under
    // corresponding category
    if (!QFile::link(lnkDir.relativeFilePath(dest), savedPath)) {
        qCritical().noquote() << "FileMan:\tFailed to symlink" << dest << "to" << savedPath;
        return {{"error", EACCES}, {"path", {}}};
    }

    qInfo().noquote() << "FileMan:\t" << src << "successfully added as" << dest;
    return {{"error", 0}, {"path", QUrl::fromLocalFile(savedPath)}};
}

// Qt provides no way (as of now) to validate hex chars
// u'<char>' is necessary to avoid implicit locale translations
static bool isHexDigit(QChar ch) noexcept
{
    const char16_t c = ch.unicode();
    return (c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'f') || (c >= u'A' && c <= u'F');
}

//
// Validate the file lnkFileName on store before removing; this needs to be -
// and allows to be - a lot more stricter than preAddValidate() for obvious
// reasons
//
static QVariantMap preRmValidate(const QString &lnkFileName, FileMan::Categories category, const bool enforcedCheck = false)
{
    // Fail early if dataDir is actually a symlink
    if (QFileInfo(FileMan::getDataDir()).isSymLink()) {
        qWarning().noquote() << "FileMan:\tStore directory is a symlink";
        return {{"lnkOk", ECANCELED}, {"lnkName", {}}, {"hashPath", {}}};
    }

    QString dir = QDir(FileMan::getDataDir())
                                .absoluteFilePath(QLatin1StringView("common"));
    qDebug().noquote() << "FileMan:\tStore path (common):" << dir;
    if (dir.isEmpty() || !QFileInfo::exists(dir)) {
        qWarning().noquote() << "FileMan:\tStore doesn't exist yet";
        return {{"lnkOk", EAGAIN}, {"lnkName", {}}, {"hashPath", {}}};
    }
    if (QFileInfo(dir).isSymLink()) {
        qWarning().noquote() << "FileMan:\t" << dir << "is a symlink";
        return {{"lnkOk", ECANCELED}, {"lnkName", {}}, {"hashPath", {}}};
    }
    const QDir storeDir(dir);

    dir = QDir(FileMan::getDataDir()).absoluteFilePath(catStr(category).toString());
    qDebug().noquote() << "FileMan:\tStore path (categorized):" << dir;
    if (dir.isEmpty() || !QFileInfo::exists(dir)) {
        qWarning().noquote() << "FileMan:\tStore doesn't exist yet";
        return {{"lnkOk", EAGAIN}, {"lnkName", {}}, {"hashPath", {}}};
    }
    if (QFileInfo(dir).isSymLink()) {
        qWarning().noquote() << "FileMan:\t" << dir << "is a symlink";
        return {{"lnkOk", ECANCELED}, {"lnkName", {}}, {"hashPath", {}}};
    }
    const QDir categoryDir(dir);

     // lnkFileName can also be a full path
#ifdef Q_OS_WIN
    // Valid Windows symlinks present in store should be having .lnk extensions
    const QString file = QFileInfo(lnkFileName).fileName()
                        + QLatin1StringView(".lnk");
#else
    const QString file = QFileInfo(lnkFileName).fileName();
#endif

    const QFileInfo lnkFile(categoryDir.absoluteFilePath(file));

    // Files under dataDir/<category> are usually symlinks
    if (!lnkFile.isSymLink() && !lnkFile.exists()) {
        // This file actually doesn't exist
        return {{"lnkOk", ENOENT}, {"lnkName", {}}, {"hashPath", {}}};
    }

    // See the target of the symlink first but don't resolve yet
    const QString target = QDir::cleanPath(categoryDir.filePath(lnkFile.symLinkTarget()));
    const QFileInfo targetFile(target);
    qDebug().noquote() << "FileMan:\tSymlink target:" << target;

    // All healthy symlinks canonically point to "dataDir/common/<hashFile>"
    if (targetFile.absolutePath() != storeDir.absolutePath()) {
        // An alien symlink in the store, disallow symlink resolution
        qWarning().noquote() << "FileMan:\t" << file << "points to forbidden path or not a symbolic link";
        return {{"lnkOk", 0}, {"lnkName", file}, {"hashPath", {}}};
    }

    // Target path may still be a crafted jump-symlink residing in the store
    if (targetFile.isSymLink()) {
        qWarning().noquote() << "FileMan:\t" << target << "is an unknown file (a symlink)";
        return {{"lnkOk", 0}, {"lnkName", file}, {"hashPath", {}}};
    }

    //
    // reaching here means provided symlink & target are both valid & safe
    // (even if it's dangling). Proceed to check the hashFile next
    //

    // Target may not exist
    if (!QFileInfo::exists(target)) {
        qWarning().noquote() << "FileMan:\t" << file << "doesn't exist on store";
        return {{"lnkOk", 0}, {"lnkName", file}, {"hashPath", {}}};
    }

    // ...or doesn't pass our expected validations, preAddValidate() handles
    // some of this already, but only when called for an enforced check since
    // this is a costly function

    if (enforcedCheck) {
        if (const int e = preAddValidate(target, category); e != 0) {
            qWarning().noquote() << "FileMan:\t" << target << "didn't pass validations";
            qDebug().noquote() << "FileMan:\tError:" << e;
            return {{"lnkOk", 0}, {"lnkName", file}, {"hashPath", {}}};
        }
    } else {
        if (!targetFile.isFile() || targetFile.size() <= 0) {
        // Regular, but unusual files; e.g. textstreams or files under /proc
            qWarning().noquote() << "FileMan:\t" << target << "is not a valid file";
            return {{"lnkOk", 0}, {"lnkName", file}, {"hashPath", {}}};
        }
    }

    // Valid files ought to have 64 characters exactly (sha256 digest length)
    const QString targetBaseName = targetFile.completeBaseName();
    constexpr int nameLength = 64;
    if (qint64 s = targetBaseName.size(); s != nameLength) {
        qWarning().noquote() << "FileMan:\t" << target << "is not a valid file in store (wrong name)";
        qDebug().noquote() << "FileMan:\tBase name size:" << s << "vs" << nameLength;
        return {{"lnkOk", 0}, {"lnkName", file}, {"hashPath", {}}};
    }

    // They also have exactly hex characters: [a-fA-F] & [0-9]
    for (QChar c : targetBaseName) {
        if (!isHexDigit(c)) {
            qWarning().noquote() << "FileMan:\t" << target << "is not a valid file in store (wrong name)";
            qDebug().noquote() << "FileMan:\tBase name" << targetBaseName << "doesn't look like a sha256 hash";
            return {{"lnkOk", 0}, {"lnkName", file}, {"hashPath", {}}};
        }
    }

    // Validate the digest against file name. This is a costly operation so
    // saving this for last and only when called for enforced check
    if (enforcedCheck) {
        if (QString h = sha256File(target); h != targetBaseName) {
            qWarning().noquote() << "FileMan:\tDigest mismatch for" << target;
            return {{"lnkOk", 0}, {"lnkName", file}, {"hashPath", {}}};
        }
    }

    return {{"lnkOk", 0}, {"lnkName", file}, {"hashPath", target}};
}

//
// Function to remove the file of specified category from store. Takes only the
// file name & not a path. Strips the file name (i.e. "basename" as on Linux)
// from a path even if supplied
//
int FileMan::rmFile(const QString &fileName, FileMan::Categories category)
{
    bool entered = false;
    int err = 0;
    for (const auto &c : std::as_const(categoryList)) {
        const auto cat = category & c;
        if (cat == 0) continue;

        entered = true;

        qDebug().noquote() << "FileMan:\tSearching category" << catStr(cat);
        const QVariantMap result = preRmValidate(fileName, cat);
        const QString linkName = result["lnkName"].toString();
        qDebug().noquote() << "FileMan:\tFile name in store:" << linkName;

        if (int e = result["lnkOk"].toInt(); e != 0 || linkName.isEmpty()) {
            qCritical().noquote() << "FileMan:\tRefusing to proceed with" << fileName;
            qDebug().noquote() << "FileMan:\tError:" << e;
            if (err != 0) err = e; continue;
        }

        // Following are all hopefully reliable operations onwards
        QDir categoryDir = QDir(FileMan::getDataDir())
                            .filePath(catStr(cat).toString());

        if (QString f = result["hashPath"].toString(); f.isEmpty()) {
            qWarning().noquote() << "FileMan:\tNo valid hashed file found in store for" << linkName;
        } else {
            // Remove the raw blob from the store first
            if (!QFile(f).remove()) {
                qCritical().noquote() << "FileMan:\tCouldn't remove" << f;
                if (err != 0) err = EACCES; continue;
            }
        }

        // Remove the symlink next
        if (!categoryDir.remove(linkName)) {
            qCritical().noquote() << "FileMan:\tCouldn't remove" << linkName;
            if (err != 0) err = EACCES; continue;
        }

        qInfo().noquote() << "FileMan:\t" << linkName << "removed successfully from store";
    }

    if (!entered) {
        qCritical().noquote() << "FileMan:\t" << category << "is an invalid category";
        err = EINVAL;
    }

    return err;
}

//
// A housekeeper for the store. Should run on application/component startup
//
int FileMan::storeHouseKeeping()
{
    QStringList contents;
    QHash<QString, QString> contentsDict;

    for (const auto &category : std::as_const(categoryList)) {
        QDir categoryDir;
        if (const QString d = mkDataDir(category); d.isEmpty()) {
            qCritical().noquote() << "FileMan:\tCouldn't create" << category << "store";
            return EACCES;
        }
        else categoryDir.setPath(d);

        // Enlist all top-level files (unsorted) in the directory. QDir::System
        // is necessary to enlist dangling symlinks
        contents = categoryDir.entryList(QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);
        if (contents.isEmpty()) {
            qWarning().noquote() << "FileMan:\t" << catStr(category) << "store doesn't have any contents yet";
            continue;
        }

        qDebug().noquote() << "FileMan:\tContents in" << categoryDir.path() << ":";
        qDebug() << contents;

        const qsizetype size = contents.size();

        // A good estimate in good circumstances
        contentsDict.reserve(size);

        // Build a dictionary of valid files in the store
        for (const auto &f : std::as_const(contents)) {
            const QVariantMap r = preRmValidate(f, category, true);
            if (r["lnkOk"].toInt() != 0) {
                qWarning().noquote() << "FileMan:\t" << f << "not available in store";
                continue;
            }

            const QString h = QFileInfo(r["hashPath"].toString()).fileName();
            if (h.isEmpty()) {
                if (!categoryDir.remove(f))
                    qWarning().noquote() << "FileMan:\tCouldn't remove" << f;

                continue;
            }

            contentsDict.insert(h, r["lnkName"].toString());
        }
    }

    // Compact the dictionary after finished building
    qDebug().noquote() << "FileMan:\tHash table keys:";
    qDebug() << contentsDict.keys();
    qDebug().noquote() << "FileMan:\tHash table values:";
    qDebug() << contentsDict.values();
    contentsDict.squeeze();

    if (contentsDict.isEmpty()) {
    // This means there's nothing in the "common" store anyway
    // Our job here is done
        qWarning().noquote() << "FileMan:\tCommon store doesn't have any valid contents";
        return 0;
    }

    QDir dataDir;
    if (const QString d = mkDataDir(); d.isEmpty()) {
    // This is most probably caused by mkDataDir(), but we still consider it
        qCritical().noquote() << "FileMan:\tCouldn't create \"common\" store";
        return EACCES;
    }
    else dataDir.setPath(d);

    contents = dataDir.entryList(QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);
    if (contents.isEmpty())
    { qWarning().noquote() << "FileMan:\tCan't access" << dataDir; return 0; }

    for (const auto &h : std::as_const(contents)) {
        if (const QString &f = contentsDict.value(h, ""); f.isEmpty()) {
            if (!dataDir.remove(h)) {
                qWarning().noquote() << "FileMan:\tCouldn't remove" << h;
                continue;
            }
        }
    }

    qInfo().noquote() << "FileMan:\tHousekeeping done on store";

    return 0;
}