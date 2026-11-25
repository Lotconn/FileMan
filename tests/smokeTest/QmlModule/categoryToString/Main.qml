import QtQml
import Lotconn.FileMan

QtObject {
    Component.onCompleted: {
        for (const cat of [FileMan.None, FileMan.Audio, FileMan.Image, FileMan.Video, FileMan.All])
            console.info(FileMan.categoryToString(cat));

        Qt.quit();
    }
}
