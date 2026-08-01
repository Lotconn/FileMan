#include <fileman>

//
// [Public, Static] A helper function usually helpful in QML. Returns file name
// as string, null if file name can't be resolved.
//
QString FileMan::getFileName(const QUrl &fileUrl) { return fileUrl.fileName(); }