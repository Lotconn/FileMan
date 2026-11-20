import QtCore
import QtQml
import Lotconn.FileMan

QtObject {
    Component.onCompleted: {
        const dataDir = FileMan.getDataDir();
        const fileComp = Qt.platform.os === "windows" ? "/image/i00.png.lnk" : "/image/i00.png";
        const urlSpec = Qt.platform.os === "windows" ? "file:///" : "file://";
        const fileUrl = Qt.resolvedUrl(urlSpec + dataDir + fileComp);

        const resAdd = FileMan.addFile(Qt.resolvedUrl("file:data/i00.png"));

        if (resAdd.status === 0) {
            const resRm = FileMan.rmFile(fileUrl);
            console.info(resRm.status_l + " " + resRm.status_b);
        }

        Qt.quit();
    }
}
