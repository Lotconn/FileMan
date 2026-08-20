#include <QtCore>
#include <fileman>
#include <qlatin1stringview.h>

#define logDebug    qDebug().noquote().nospace()
#define logInfo     qInfo().noquote().nospace()
#define logWarning  qWarning().noquote().nospace()
#define logCritical qCritical().noquote().nospace()

using Qt::StringLiterals::operator""_L1;