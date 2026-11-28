import QtQml
import Lotconn.FileMan

QtObject {
    Component.onCompleted: {
        const dataDir = FileMan.storeDir();
        const fileComp = Qt.platform.os === "windows" ? "/images/i00.png.lnk" : "/images/i00.png";
        const fileUrl = Qt.resolvedUrl(dataDir + fileComp);

        const resAdd = FileMan.addFile(Qt.resolvedUrl("file:data/i00.png"));

        if (resAdd.status === 0) {
            const resRm = FileMan.rmFile(fileUrl);
            console.info(resRm.status_l + " " + resRm.status_b);
        }

        Qt.quit();
    }
}
