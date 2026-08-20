import QtCore
import QtQml

QtObject {
    Component.onCompleted: {
        console.info(fileman.getDataDir());
        Qt.quit();
    }
}
