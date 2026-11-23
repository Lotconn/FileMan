import QtQml

QtObject {
    Component.onCompleted: {
        const dataDir = fileman.storeDir();
        const fileComp = Qt.platform.os === "windows" ? "/image/i00.png.lnk" : "/image/i00.png";
        const urlSpec = Qt.platform.os === "windows" ? "file:///" : "file://";
        const fileUrl = Qt.resolvedUrl(urlSpec + dataDir + fileComp);

        const resAdd = fileman.addFile(Qt.resolvedUrl("file:data/i00.png"));

        if (resAdd.status === 0) {
            const resRm = fileman.rmFile(fileUrl);
            console.info(resRm.status_l + " " + resRm.status_b);
        }

        Qt.quit();
    }
}
