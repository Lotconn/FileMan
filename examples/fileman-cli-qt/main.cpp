/*

                          MIT License

                      Copyright HasanEsa 2026

This is an example interactive cli app demonstrating the integration of FileMan
class in a Qt/C++ implementation. Using the app should be starightforward, just
follow the prompts. When providing a file URL, append "file://" for absolute
paths and "file:" for relative.

The key directives to extract from this example for library integration are
highlighted with [KEY] inline comment on the preceeding line. For a subsequent
block, the highlighted part starts with [KEY:BEGIN] and ends with [KEY:END].
Additional notes are appended to the specifier like [KEY:BEGIN:extra comments]
or [KEY::extra comments].

*/

#include <QtCore>
#include <QTextStream>

//[KEY:BEGIN:Change this to <HasanEsa/FileMan/fileman.cpp> for your app]
#include <QVariant>
#include <fileman.hpp>
//[KEY:END]

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("fileman-cli-qt");

//[KEY]
    FileMan fileman;
    QTextStream in(stdin);

    while (!in.atEnd()) {
        // 1. Operation Selection
        qInfo().noquote() <<
            "Select operation:\n[1] Add\n[2] Remove\n[3] Housekeeping\n[4] Extract file name";
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        int op = line.toInt();
        if (op < 1 || op > 4) { qInfo() << "Invalid operation."; continue; }

        bool useDefault = false;

//[KEY]
        FileMan::Categories category;
        if (! (op == 3 || op == 4)) {
        // 2. Category Selection
            qInfo().noquote() <<
                "Select Category:\n[1] Audio\n[2] Image\n[3] Video\n[4] Audio & Video\n[5] All";
            line = in.readLine().trimmed();
            if (line.isEmpty()) continue;

//[KEY:BEGIN:Alternatively, pass FileMan::Category enum directly]
            if (line == "1") category = FileMan::Audio;
            else if (line == "2") category = FileMan::Image;
            else if (line == "3") category = FileMan::Video;
            else if (line == "4") category = (FileMan::Audio | FileMan::Video);
            else if (line == "5") useDefault = true;
            else { qInfo() << "Invalid category."; continue; }
//[KEY:END]

        }

        // 3. Execute Operation
        switch (op) {
            case 1: {
                qInfo() << "Provide file URL:";
                QString fileUrl = in.readLine().trimmed();
                if (fileUrl.isEmpty())
                { qInfo() << "No URL provided."; continue; }

                QVariantMap result;

//[KEY:BEGIN:addFile() usage]
                if (useDefault)
                    result = fileman.addFile(QUrl(fileUrl));
                else result = fileman.addFile(QUrl(fileUrl), category);
//[KEY:END]

                qInfo() << "Program returned saved path:"
                        << result["path"].toString();
                if (int e = result["error"].toInt(); e != 0)
                    qInfo() << "addFile() return:" << e;
                break;
            }
            case 2: {
                qInfo() << "Provide file name:";
                QString fileUrl = in.readLine().trimmed();
                if (fileUrl.isEmpty())
                { qInfo() << "Nothing provided."; continue; }

//[KEY:rmFile() usage]
                int e = useDefault ? fileman.rmFile(fileUrl) :
                                    fileman.rmFile(fileUrl, category);
                qInfo() << "rmFile() return:" << e;
                break;
            }
            case 3: {

//[KEY:storeHouseKeeping() usage]
                int e = fileman.storeHouseKeeping();
                qInfo() << "storeHouseKeeping() return:" << e;
                break;
            }
            case 4: {
                qInfo() << "Provide file URL:";
                QString fileUrl = in.readLine().trimmed();

//[KEY:getFileName() usage]
                qInfo() << "Converted from getFileName():"
                        << fileman.getFileName(QUrl(fileUrl));
                break;
            }
            default:
                qInfo() << "No operation to perform";
                return 0;
        }
    }

    return 0;
}