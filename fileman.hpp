#ifndef HASANESA_FILEMAN_FILEMAN_HPP
#define HASANESA_FILEMAN_FILEMAN_HPP

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUrl>
#include <QVariant>
#include <QtQmlIntegration/qqmlintegration.h>

class FileMan : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")
    QML_ELEMENT
    QML_SINGLETON

public:
    enum Category : unsigned int
    {
        Audio   = 1 << 0,
        Image   = 1 << 1,
        Video   = 1 << 2,
        All     = Audio | Image | Video
    };
    Q_DECLARE_FLAGS(Categories, Category)
    Q_ENUM(Category)
    Q_FLAG(Categories)

    explicit FileMan(QObject *parent = nullptr);

    static FileMan *s_singletonInstance;
    static FileMan *create(QQmlEngine *, QJSEngine *)
    {
        Q_ASSERT_X(s_singletonInstance, "FileMan::create",
                "s_singletonInstance must be set before the QML engine loads");
        return s_singletonInstance;
    }

    Q_INVOKABLE static QString getDataDir();
    Q_INVOKABLE QString getFileName(const QUrl &fileUrl);
    [[nodiscard]] Q_INVOKABLE QVariantMap addFile(const QUrl &fileUrl, FileMan::Categories category = FileMan::All);
    [[nodiscard]] Q_INVOKABLE int rmFile(const QString &fileName, FileMan::Categories category = FileMan::All);
    [[nodiscard]] Q_INVOKABLE int storeHouseKeeping();
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FileMan::Categories)

#endif  // HASANESA_FILEMAN_FILEMAN_HPP