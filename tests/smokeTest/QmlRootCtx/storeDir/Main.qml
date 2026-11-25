import QtQml

QtObject {
    Component.onCompleted: {
        console.info(fileman.storeDir());
        Qt.quit();
    }
}
