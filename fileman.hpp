#pragma once

#include <QDir>
#include <QObject>
#include <QQmlEngine>
#include <QSharedDataPointer>
#include <memory>

#if defined(FM_STATIC_LIB)
#define FM_SYM_ATTR
#elif defined(FM_LIB_BUILD)
#define FM_SYM_ATTR Q_DECL_EXPORT
#else
#define FM_SYM_ATTR Q_DECL_IMPORT
#endif

class AddFileType;
class RmFileType;

struct alignas(8) FM_SYM_ATTR add_file_t {
  Q_GADGET
  Q_PROPERTY(int status READ status CONSTANT)
  Q_PROPERTY(QUrl location READ location CONSTANT)
  QML_ANONYMOUS

public:
  add_file_t();
  add_file_t(int status, const QUrl &location);

  add_file_t(const add_file_t &);
  add_file_t &operator=(const add_file_t &);

  add_file_t(add_file_t &&) noexcept;
  add_file_t &operator=(add_file_t &&) noexcept;

  ~add_file_t();

  [[nodiscard]] int  status() const;
  [[nodiscard]] QUrl location() const;
  [[nodiscard]] bool isNull() const noexcept;

private:
  QExplicitlySharedDataPointer<AddFileType> d;
};

struct alignas(8) FM_SYM_ATTR rm_file_t {
  Q_GADGET
  Q_PROPERTY(int status_l READ status_l CONSTANT)
  Q_PROPERTY(int status_h READ status_h CONSTANT)
  QML_ANONYMOUS

public:
  rm_file_t();
  rm_file_t(int status_l, int status_h);

  rm_file_t(const rm_file_t &);
  rm_file_t &operator=(const rm_file_t &);

  rm_file_t(rm_file_t &&) noexcept;
  rm_file_t &operator=(rm_file_t &&) noexcept;

  ~rm_file_t();

  [[nodiscard]] int  status_l() const;
  [[nodiscard]] int  status_h() const;
  [[nodiscard]] bool isNull() const noexcept;

private:
  QExplicitlySharedDataPointer<RmFileType> d;
};

class FM_SYM_ATTR FileMan : public QObject {
  Q_OBJECT
  Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")
  QML_ELEMENT
  QML_SINGLETON

public:
  explicit FileMan(QObject *parent = nullptr);
  ~FileMan() override;

  static FileMan *create(QQmlEngine *, QJSEngine *);
  static FileMan &instance();

  //////////////////////////////////////////////////////////////////////////////
  //  Type/Enum definitions
  //////////////////////////////////////////////////////////////////////////////

  enum Category : quint8 {
    None  = 0x00,
    Audio = 0x01,
    Image = 0x02,
    Video = 0x04,
    All   = Audio | Image | Video
  };
  Q_DECLARE_FLAGS(Categories, Category)
  Q_FLAG(Categories)

  enum class Errno : quint8 {
    Success          = 0,
    Busy             = EBUSY,
    Canceled         = ECANCELED,
    Exists           = EEXIST,
    FileTooBig       = EFBIG,
    Invalid          = EINVAL,
    IOError          = EIO,
    Loop             = ELOOP,
    NotFound         = ENOENT,
    NotSupported     = EOPNOTSUPP,
    PermissionDenied = EPERM,
    TryAgain         = EAGAIN
  };
  Q_ENUM(Errno)

  //////////////////////////////////////////////////////////////////////////////
  //  Function definitions
  //////////////////////////////////////////////////////////////////////////////

  Q_INVOKABLE static QString getFileName(const QUrl &fileUrl);

  Q_INVOKABLE [[nodiscard]] QString categoryToString(Category category) const;
  Q_INVOKABLE [[nodiscard]] QString getCategoryDir(Category category) const;
  Q_INVOKABLE [[nodiscard]] QString getDataDir() const;

  Q_INVOKABLE add_file_t addFile(const QUrl &fileUrl, Categories categories = All);
  Q_INVOKABLE int        refreshStore();
  Q_INVOKABLE rm_file_t  rmFile(const QUrl &fileUrl);

private:
  Q_DISABLE_COPY_MOVE(FileMan)
  class Private;

  //////////////////////////////////////////////////////////////////////////////
  //  Variable/Object definitions
  //////////////////////////////////////////////////////////////////////////////

  static FileMan                *s_singletonInstance;
  const std::unique_ptr<Private> d;

  //////////////////////////////////////////////////////////////////////////////
  //  Type definitions
  //////////////////////////////////////////////////////////////////////////////

  struct alignas(8) pre_add_validate_t {
    int        status = {};
    Categories categories;
  };

  struct alignas(64) pre_rm_validate_t {
    int     status    = {};
    QString lnkEntry  = {};
    QString hashEntry = {};
  };

  struct alignas(32) rev_lookup_symlink_t {
    int     status   = {};
    QString location = {};
  };

  struct alignas(16) validate_store_t {
    int                  d_status = {};
    int                  h_status = {};
    QHash<Category, int> c_status;
  };

  //////////////////////////////////////////////////////////////////////////////
  //  Function definitions
  //////////////////////////////////////////////////////////////////////////////

  static auto createPathComp(const QString &comp1, const QString &comp2) -> QString;
  static auto resolveDataDir() -> QString;

  auto acquireStoreLock() -> int;
  auto clearAllEntries() -> void;
  auto dumpAllEntries() -> void;
  auto fixDirHealth(const QString &dir, int error, FileMan::Category category = FileMan::None)
      -> bool;
  auto isDictEmpty(const QHash<QString, QString> &dict) -> bool;
  auto lookupHashByLink(const QString &lnkEntry) -> QString;
  auto lookupLinkByHash(const QString &hash) -> QString;
  auto mkDataDir() -> bool;
  auto mkDataDir(FileMan::Category category) -> bool;
  auto preAddValidate(const QFileInfo &file) -> pre_add_validate_t;
  auto preRmValidate(const QString &lnkPath, bool enforcedCheck = false) -> pre_rm_validate_t;
  auto registerEntry(const QString &lnkEntry, const QString &hash) -> void;
  auto releaseStoreLock() -> void;
  auto removeEntry(const QString &lnkEntry, const QString &hash) -> void;
  auto revLookupSymlink(const QDir &searchDir, const QString &targetPath) -> rev_lookup_symlink_t;
  auto squeezeDicts() -> void;
  auto validateStore() -> validate_store_t;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FileMan::Categories)