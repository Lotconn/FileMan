#pragma once

#include <QHash>
#include <QLoggingCategory>
#include <QSaveFile>

#include <fileman>

Q_DECLARE_LOGGING_CATEGORY(fileman)

#define logDebug    qCDebug(fileman).noquote() << __func__ << ":debug:"_L1
#define logInfo     qCInfo(fileman).noquote() << __func__ << ":info:"_L1
#define logWarning  qCWarning(fileman).noquote() << __func__ << ":warning:"_L1
#define logCritical qCCritical(fileman).noquote() << __func__ << ":error:"_L1

// Allows shorthand QLatin1StringView ""_L1 operators
// Included in this internal header for all sources to utilize
using Qt::StringLiterals::operator""_L1;

////////////////////////////////////////////////////////////////////////////////
//  Type definitions
////////////////////////////////////////////////////////////////////////////////

struct alignas(64) hash_and_stage_t {
  int     status;
  QString hash;
  QString stagedPath;
};

////////////////////////////////////////////////////////////////////////////////
//  Function definitions
////////////////////////////////////////////////////////////////////////////////

auto compErrnoSeverity(int error1, int error2) -> int;
auto dirHealth(const QString &dirPath) -> int;
auto diskFreeCheck(const QFileInfo &file, const QDir &dir) -> int;
auto errnoSeverity(int e) -> int;
auto hashFile(const QString &path) -> QString;
auto hashAndStageFile(QFile &srcFile, QSaveFile &stageFile) -> hash_and_stage_t;
auto isHexDigit(QChar ch) noexcept -> bool;
auto lowestSetFlag(FileMan::Categories categories) -> FileMan::Category;
auto renameFile(const QString &fileName, const QDir &location) -> QString;
auto setDirPerms(const std::filesystem::path &path, std::error_code &ec) -> bool;
auto validateFile(const QFileInfo &file) -> int;
auto validateFile(const QFileInfo &file, qint64 fileSize) -> int;

////////////////////////////////////////////////////////////////////////////////
//  Variable/Object definitions
////////////////////////////////////////////////////////////////////////////////

// 64KiB is a good chunk size for almost all purposes including network
// files, although 1MiB would've been more ideal for local disk files
inline constexpr qint32 ioChunkSize = 65536;
