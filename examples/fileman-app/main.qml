/*

                          MIT License

                      Copyright HasanEsa 2026

This is an example app demonstrating legacy use of embedding the FileMan object
in QML with context properties. Although this approach is discouraged in favor
of QML modules, most apps still follow this procedure.

The key directives to extract from this example for library integration are
highlighted with [KEY] inline comment on the preceeding line. For a subsequent
block, the highlighted part starts with [KEY:BEGIN] and ends with [KEY:END].
Additional notes are appended to the specifier like [KEY:BEGIN:extra comments]
or [KEY::extra comments].

*/

import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

//[KEY:Can also be followed by the registered version number]
import HasanEsa.FileMan.Enums

ApplicationWindow {
    id: mainWindow
    visible: true
    height: 150
    width: 250
    minimumWidth: 250
    minimumHeight: 150

    property var audioList: []
    property var imageList: []
    property var videoList: []
    property var category:  FileMan.All

    Component.onCompleted: {
        settings.category = "Audio"
        var list = settings.value("list", JSON.stringify(audioList))
        try {
            audioList = JSON.parse(list)
        } catch(_) {}

        settings.category = "Image"
        list = settings.value("list", JSON.stringify(imageList))
        try {
            imageList = JSON.parse(list)
        } catch(_) {}

        settings.category = "Video"
        list = settings.value("list", JSON.stringify(videoList))
        try {
            videoList = JSON.parse(list)
        } catch(_) {}

        settings.category = "Window"
        mainWindow.width    = settings.value("width",   mainWindow.width)
        mainWindow.height   = settings.value("height",  mainWindow.height)
        mainWindow.x        = settings.value("x",       mainWindow.x)
        mainWindow.y        = settings.value("y",       mainWindow.y)

//[KEY:storeHouseKeeping() usage]
        var e = fileman.storeHouseKeeping()
        console.debug("Fileman::storeHouseKeeping() return:", e)
    }

    Component.onDestruction: {
        settings.category = "Audio"
        settings.setValue("list", JSON.stringify(audioList || []))

        settings.category = "Image"
        settings.setValue("list", JSON.stringify(imageList || []))

        settings.category = "Video"
        settings.setValue("list", JSON.stringify(videoList || []))

        settings.category = "Window"
        settings.setValue("width",  mainWindow.width)
        settings.setValue("height", mainWindow.height)
        settings.setValue("x",      mainWindow.x)
        settings.setValue("y",      mainWindow.y)

        settings.sync()
    }

    Settings { id: settings }

    FileDialog {
        id: fileDialog
        title: qsTr("Please choose an audio file")
        currentFolder: StandardPaths.writableLocation(StandardPaths.MusicLocation)
        fileMode: FileDialog.OpenFiles
        nameFilters: [
            "Audio files (*.aac *.ac3 *.alac *.amr *.flac *.m4a *.mp3 *.ogg *.opus *.wav *.wma *.weba)",
            "Image files (*.jpg *.jpeg *.png *.svg *.tiff *.tif *.gif *.avif *.bmp *.dng *.cr2 *.heic *.heif *.ico *.nef *.webp)",
            "Video files (*.mp4 *.mkv *.webm *.avi *.flv *.mov *.m4v *.ogv *.wmv *.3gp *.3g2)",
            "All files (*)"
        ]

        onAccepted: {
            category    = selectedNameFilter.index == 0 ? FileMan.Audio
                        : selectedNameFilter.index == 1 ? FileMan.Image
                        : selectedNameFilter.index == 2 ? FileMan.Video
                        : FileMan.All

            for (var i = 0; i < selectedFiles.length; i++) {
                var fileUrl = selectedFiles[i]
//[KEY:addFile() usage]
                var result = fileman.addFile(fileUrl, category)

                if (result.error !== 0) {
                    console.warn("Fileman::addFile() failed with error:", result.error)
                    continue
                }

//[KEY:getFileName() usage]
                var fileName = fileman.getFileName(fileUrl)
                var savedPath = result.path

                var newList = mainWindow.audioList.slice()
                newList.push({ "name": fileName, "path": savedPath })
                mainWindow.audioList = newList
                audioListCb.currentIndex = mainWindow.audioList.length - 1
            }
        }
    }

    Page {
        anchors.fill: parent

        Flickable {
            anchors.fill: parent
            focus: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Keys.onUpPressed: scrollBar.decrease()
            Keys.onDownPressed: scrollBar.increase()

            ScrollBar.vertical: ScrollBar {
                id: scrollBar
            }

            contentWidth: parent.width
            contentHeight: col.implicitHeight + 20

            ColumnLayout {
                id: col
                spacing: 10
                width: parent.width - 40
                anchors.top: parent.top
                anchors.topMargin: 10
                anchors.horizontalCenter: parent.horizontalCenter

                ComboBox {
                    id: audioListCb
                    model: mainWindow.audioList
                    Layout.fillWidth: true
                    textRole: "name"
                }

                RowLayout {
                    spacing: 10
                    Layout.alignment: Qt.AlignHCenter

                    Button {
                        text: "Add"
                        onClicked: fileDialog.open()
                    }

                    Button {
                        text: "Remove"

                        onClicked: {
                            var rmIdx = audioListCb.currentIndex
//[KEY:rmFile() usage with default category]
                            var status = fileman.rmFile(mainWindow.audioList[rmIdx].name)
                            if (status !== 0) {
                                console.warn("Fileman::rmFile() failed with error:", status)
                                return
                            }

                            var newList = mainWindow.audioList.slice()
                            newList.splice(rmIdx, 1)
                            mainWindow.audioList = newList
                        }
                    }
                }
            }
        }
    }

    Shortcut {
        sequences: [StandardKey.Quit]
        context: Qt.ApplicationShortcut
        onActivated: Qt.quit()
    }
}
