import QtQml
import Lotconn.FileMan

QtObject {
    Component.onCompleted: {
        console.info(FileMan.storeDir());
        Qt.quit();
    }
}
