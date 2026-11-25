#include <QCoreApplication>

#include <FileMan>

#define logDebug    qDebug().noquote().nospace()
#define logInfo     qInfo().noquote().nospace()
#define logWarning  qWarning().noquote().nospace()
#define logCritical qCritical().noquote().nospace()

using Qt::StringLiterals::operator""_L1;
using Qt::StringLiterals::operator""_s;