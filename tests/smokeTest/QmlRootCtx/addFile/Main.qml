import QtCore
import QtQml
import Lotconn.FileMan.Enums

QtObject {
    Component.onCompleted: {
        const a00 = Qt.resolvedUrl("file:data/a00.ac3");
        const i00 = Qt.resolvedUrl("file:data/i00.png");
        const v00 = Qt.resolvedUrl("file:data/v00.mp4");
        const a01 = Qt.resolvedUrl("file:data/a01.flac");

        let i = 0;
        for (const res of [fileman.addFile(a00, FileMan.Audio), fileman.addFile(i00, FileMan.Video | FileMan.Image), fileman.addFile(a01, FileMan.Audio | FileMan.Video), fileman.addFile(v00)])
            console.info(res.status + " " + res.location);

        Qt.quit();
    }
}
