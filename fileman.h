#ifndef FILEMAN_H
#define FILEMAN_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariant>

#include "scoped-enum-bitops.h"

class FileMan : public QObject
{
    Q_OBJECT

public:
    enum Category : unsigned int
    {
        Audio = 1 << 0,
        Image = 1 << 1,
        Video = 1 << 2,
    };
    Q_FLAG(Category)

    explicit FileMan(QObject *parent = nullptr);

    Q_INVOKABLE static QString getDataDir();
    Q_INVOKABLE QString getFileName(const QUrl &fileUrl);
    [[nodiscard]] Q_INVOKABLE QVariantMap addFile(const QUrl &fileUrl, Category category);
    [[nodiscard]] Q_INVOKABLE int rmFile(const QString &fileName, Category category);
    [[nodiscard]] Q_INVOKABLE int storeHouseKeeping();
};

ENABLE_BITWISE_OPS_FOR_ENUM(FileMan::Category); // <= possible for type_to_eat_semicolon ;)

#endif  // FILEMAN_H