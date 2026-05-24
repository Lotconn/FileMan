#include <QtCore>
#include <QTextStream>
#include <QVariant>
#include "fileman.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("myapp");

    FileMan fileman;
    QTextStream in(stdin);

    while (!in.atEnd()) {
        // 1. Category Selection
        qInfo().noquote() <<
            "Select Category:\n[1] Audio\n[2] Image\n[3] Video\n[4] Audio & Video\n[5] All";
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        FileMan::Categories category;
        bool useDefault = false;
        if (line == "1") category = FileMan::Audio;
        else if (line == "2") category = FileMan::Image;
        else if (line == "3") category = FileMan::Video;
        else if (line == "4") category = (FileMan::Audio | FileMan::Video);
        else if (line == "5") useDefault = true;
        else { qInfo() << "Invalid category."; continue; }

        // 2. Operation Selection
        qInfo().noquote() << "Select operation:\n[1] Add\n[2] Remove\n[3] Housekeeping";
        line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        int op = line.toInt();
        if (op < 1 || op > 3) { qInfo() << "Invalid operation."; continue; }

        // 3. File URL Input
        qInfo() << "Provide file URL:";
        QString fileUrl = in.readLine().trimmed();
        if (fileUrl.isEmpty()) { qInfo() << "No URL provided."; continue; }

        qInfo() << "Converted from getFileName():" << fileman.getFileName(QUrl(fileUrl));

        // 4. Execute Operation
        switch (op) {
            case 1: {
                QVariantMap result;
                if (useDefault)
                    result = fileman.addFile(QUrl(fileUrl));
                else result = fileman.addFile(QUrl(fileUrl), category);
                qInfo() << "Program returned saved path:" << result["path"].toString();
                if (int e = result["error"].toInt(); e != 0) return e;
                break;
            }
            case 2: {
                if (int e = useDefault ? fileman.rmFile(fileUrl) : fileman.rmFile(fileUrl, category)) return e;
                break;
            }
            case 3: {
                int e = fileman.storeHouseKeeping();
                qInfo() << "storeHouseKeeping() return:" << e;
                return e;
            }
            default:
                qInfo() << "No operation to perform";
                return 0;
        }
    }

    return 0;
}