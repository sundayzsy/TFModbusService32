#ifndef PROTOCOLJSON_H
#define PROTOCOLJSON_H

#include <QObject>
#include <QMap>
#include "commondefine.h"

class ProtocolJson : public QObject
{
    Q_OBJECT
public:
    explicit ProtocolJson(QObject *parent = nullptr);
    void loadJson(const QString &filePath);
    QMap<quint16,SignalSturct> getDataStructMap();

    int getReadRegisterCounts();
    int getWriteRegisterCounts();
    int getAllSignalCounts();
    quint16 getServerAddress();
    quint16 getReadStartAddress();
    quint16 getWriteStartAddress();
    quint8 getFunctionCode();

private:
    void saveJson();    //test
    void resetData();

private:
    QMap<quint16,SignalSturct> m_dataMap;
    int m_readRegisterCounts;
    int m_writeRegisterCounts;
    int m_allSignalCounts;
    quint16 m_serverAddress;
    quint16 m_readStartAddress;
    quint16 m_writeStartAddress;
    quint8 m_functionCode;
};

#endif // PROTOCOLJSON_H
