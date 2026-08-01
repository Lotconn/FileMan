#include "fileman_p.hpp"

//
// Constructs a dictionary of category name -> categoryDir. Returns a
// QHash dictionary and is never empty.
//
QHash<QString, QString>
FileMan::Private::createCategoryDirList(const QString &dataDir) {

  using Qt::StringLiterals::operator""_L1;

  return {{"audio"_L1, FileMan::createPathComp(dataDir, "audio"_L1)},
          {"image"_L1, FileMan::createPathComp(dataDir, "image"_L1)},
          {"video"_L1, FileMan::createPathComp(dataDir, "video"_L1)}};
}

//
// Constructs a dictionary of FileMan::Category -> categoryDir.
// Returns a QHash dictionary and is never empty.
//
QHash<FileMan::Category, FileMan::Private::category_info_t>
FileMan::Private::createCategoryList(const QString &dataDir) {

  using Qt::StringLiterals::operator""_L1;

  return {{FileMan::Audio,
           {"audio"_L1, FileMan::createPathComp(dataDir, "audio"_L1)}},
          {FileMan::Image,
           {"image"_L1, FileMan::createPathComp(dataDir, "image"_L1)}},
          {FileMan::Video,
           {"video"_L1, FileMan::createPathComp(dataDir, "video"_L1)}}};
}
