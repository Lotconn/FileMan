#pragma once

#include <QLockFile>
#include <QMutex>
#include <QReadWriteLock>

#include <fileman>

class FileMan::Private {
public:
  struct alignas(64) category_info_t {
    QString name     = {};
    QString location = {};
  };

  static auto createCategoryList(const QString &dataDir) -> QHash<Category, category_info_t>;
  static auto createCategoryDirList(const QString &dataDir) -> QHash<QString, QString>;

  QReadWriteLock          dictLock;
  QHash<QString, QString> hashToLinkDict;
  QHash<QString, QString> linkToHashDict;
  std::atomic<bool>       storeManaged = false;

  QString                          dataDir;
  QHash<QString, QString>          categoryDirs;
  QHash<Category, category_info_t> categoryList;
  QString                          hashDir;
  QMutex                           storeMutex;
  std::unique_ptr<QLockFile>       storeLock;
};

class AddFileType : public QSharedData {
public:
  int  status = 0;
  QUrl location;
};

class RmFileType : public QSharedData {
public:
  int status_l = 0;
  int status_h = 0;
};