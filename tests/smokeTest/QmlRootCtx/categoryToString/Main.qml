import QtCore
import QtQml
import Lotconn.FileMan.Enums

QtObject {
    Component.onCompleted: {
        for (const cat of [FileMan.None, FileMan.Audio, FileMan.Image, FileMan.Video, FileMan.All])
            console.info(fileman.categoryToString(cat));

        Qt.quit();
    }
}
