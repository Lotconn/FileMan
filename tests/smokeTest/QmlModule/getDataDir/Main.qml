import QtCore
import QtQml
import Lotconn.FileMan

QtObject {
    Component.onCompleted: {
        console.info(FileMan.getDataDir());
        Qt.quit();
    }
}
