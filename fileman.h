#ifndef FILEMAN_H
#define FILEMAN_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariant>

class FileMan : public QObject
{
    Q_OBJECT

public:
    enum Category { Audio, Image };
    Q_ENUM(Category)

    explicit FileMan(QObject *parent = nullptr);

    Q_INVOKABLE static QString getDataDir();
    Q_INVOKABLE QString getFileName(const QUrl &fileUrl);
    [[nodiscard]] Q_INVOKABLE QVariantMap addFile(const QUrl &fileUrl, Category category);
    [[nodiscard]] Q_INVOKABLE int rmFile(const QString &fileName, Category category);
    [[nodiscard]] Q_INVOKABLE int storeHouseKeeping();
};

#endif  // FILEMAN_H