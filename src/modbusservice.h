#ifndef MODBUSSERVICE_H
#define MODBUSSERVICE_H

#include <QObject>
#include <QModbusClient>
#include <QTimer>
#include "commondefine.h"
#include "protocoljson.h"

class ModBusService : public QObject
{
    Q_OBJECT
public:
    explicit ModBusService(QObject *parent = nullptr);

signals:
    void sig_setPLCMapValue(const QString &strKey, const QString &strValue);
    void sig_setConnected(bool isConnected);

private:
    QModbusDataUnit readRequest(quint16 qRegAddr, int iRegCount) const;
    QModbusDataUnit writeRequest(quint16 qRegAddr, int iRegCount) const;
    void readRegister();
    void writeRegister();

    //qRegAddr：寄存器地址  qRegValue：寄存器值
    void updateParamValue(quint16 qRegAddr, quint64 qRegValue);

    /* 从寄存器值中获取指定位置、长度的值
     * regValue: 整个寄存器读取的值
     * valuePos: 获取值的起始位置
     * valueSize: 获取值的长度
     * paramValue: 输出参数值
    */
    bool getParamValue16(quint16 regValue, quint16 valuePos, quint16 valueSize, quint16 &paramValue);
    bool getParamValue32(quint32 regValue, quint16 valuePos, quint16 valueSize, quint32 &paramValue);
    bool getParamValue64(quint64 regValue, quint16 valuePos, quint16 valueSize, quint64 &paramValue);

    /* 设置寄存器某位置的数值
     * oldRegValue: 寄存器读取的值
     * valuePos: 需求获取值的起始位置
     * valueSize: 获取值的长度
     * setValue: 设置的值
     * newRegValue: 输出新的寄存器值
    */
    bool setParamValue16(quint16 oldRegValue, quint16 valuePos, quint16 valueSize, quint16 setValue, quint16 &newRegValue);
    bool setParamValue32(quint32 oldRegValue, quint16 valuePos, quint16 valueSize, quint32 setValue, quint32 &newRegValue);
    bool setParamValue64(quint64 oldRegValue, quint16 valuePos, quint16 valueSize, quint64 setValue, quint64 &newRegValue);

    QVector<quint16> getWriteRegValues(quint16 qRegAddr);

    //写寄存器数据到Redis
    void readRegister2Redis();

    //Redis到读寄存器
    void redis2WriteRegister();

private slots:
    void slot_recvTimeout();
    void slot_readReady();
    void slot_reconnection();

private:
    void initConnection();
    void reConnection();
    void initJsonFile();
    void initReadMap();
    void initWriteMap();
    bool isEqualString(const QString &str1, const QString &str2);
    void printData();

private:
    QModbusClient *m_modbusDevice;
    QTimer *m_recvTimer;
    QTimer *m_reconnectionTimer;
    ProtocolJson m_jsonFile;
    SignalProtocolParam m_protocolParam;  //协议参数

    //保存参数Map Key:寄存器地址 QList<SignalParameter>寄存器下对应的参数列表
    QMap<quint16, SignalSturct> m_signalParamMap;

    QMap<QString, QString> m_readMap;
    QMap<QString, QString> m_writeMap;

    int m_debugType; //调试类型 0：不输出 1：按寄存器地址输出 2：按每个数据输出
};

#endif // MODBUSSERVICE_H
