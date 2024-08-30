#include "modbusservice.h"
#include <QCoreApplication>
#include <QSettings>
#include <QSerialPort>
#include <QModbusRtuSerialMaster>
#include <QModbusTcpClient>
#include <QFile>
#include <QUrl>
#include <QDebug>
#include <math.h>
#include <QRandomGenerator>

ModBusService::ModBusService(QObject *parent) : QObject(parent),
    m_modbusDevice(nullptr),
    m_recvTimer(nullptr),
    m_reconnectionTimer(nullptr)
{
    initJsonFile();

    m_recvTimer = new QTimer(this);
    m_recvTimer->setInterval(100);
    connect(m_recvTimer, &QTimer::timeout, this, &ModBusService::slot_recvTimeout);

    m_reconnectionTimer = new QTimer(this);
    m_reconnectionTimer->setInterval(500);
    connect(m_reconnectionTimer, &QTimer::timeout, this, &ModBusService::slot_reconnection);

    initConnection();

    m_reconnectionTimer->start();
}

QModbusDataUnit ModBusService::readRequest(quint16 qRegAddr, int iRegCount) const
{
    return QModbusDataUnit(QModbusDataUnit::HoldingRegisters, qRegAddr, iRegCount);
}

QModbusDataUnit ModBusService::writeRequest(quint16 qRegAddr, int iRegCount) const
{
    return QModbusDataUnit(QModbusDataUnit::HoldingRegisters, qRegAddr, iRegCount);
}

void ModBusService::readRegister()
{
    if (!m_modbusDevice)
        return;

    QMap<quint16, SignalSturct>::iterator itr = m_signalParamMap.begin();
    while(itr != m_signalParamMap.end())
    {
        quint16 qRegAddr = itr.key();
        int iRegBitLengh = itr.value().iRegBitLengh;
        if (auto *reply = m_modbusDevice->sendReadRequest(readRequest(qRegAddr, iRegBitLengh/16), m_protocolParam.uServerAddr))
        {
            if (!reply->isFinished())
                connect(reply, &QModbusReply::finished, this, &ModBusService::slot_readReady);
            else
                delete reply; // broadcast replies return immediately
        }
        else
        {
            qDebug()<<"Read error: " + m_modbusDevice->errorString();
        }
        itr++;
    }
}

void ModBusService::writeRegister()
{
    if (!m_modbusDevice)
        return;

    QMap<quint16, SignalSturct>::iterator itr = m_signalParamMap.begin();
    while(itr != m_signalParamMap.end())
    {
        quint16 qRegAddr = itr.key();
        int iRegBitLengh = itr.value().iRegBitLengh;
        bool bIsReadReg = itr.value().bIsReadReg;
        if(bIsReadReg)
        {
            itr++;
            continue;
        }

        QModbusDataUnit writeUnit = writeRequest(qRegAddr, iRegBitLengh/16);
        QVector<quint16> mList = getWriteRegValues(qRegAddr);
        writeUnit.setValues(mList);

        if (auto *reply = m_modbusDevice->sendWriteRequest(writeUnit, m_protocolParam.uServerAddr))
        {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, this, [this, reply](){
                    if (reply->error() == QModbusDevice::ProtocolError)
                    {
                        qDebug()<<QString("Write response error: %1 (Mobus exception: 0x%2)")
                                        .arg(reply->errorString())
                                        .arg(reply->rawResult().exceptionCode());
                    }
                    else if (reply->error() != QModbusDevice::NoError)
                    {
                        qDebug()<<QString("Write response error: %1 (code: 0x%2)")
                                        .arg(reply->errorString())
                                        .arg(reply->error(),-1,16);
                    }
                    reply->deleteLater();
                });
            }
            else
            {
                // broadcast replies return immediately
                reply->deleteLater();
            }
        }
        else
        {
            qDebug()<<"Write error: " + m_modbusDevice->errorString();
        }
        itr++;
    }
}

void ModBusService::updateParamValue(quint16 qRegAddr, quint64 qRegValue)
{
    //寄存器到SignalMap
    QMap<quint16, SignalSturct>::iterator itr = m_signalParamMap.find(qRegAddr);
    if(itr != m_signalParamMap.end())
    {
        int iRegBitLengh = itr.value().iRegBitLengh;
        QList<SignalParameter> mList = itr.value().spList;
        if(iRegBitLengh == 16)
        {
            for(int j=0; j<mList.size();j++)
            {
                quint16 qPos = mList.at(j).uBitPos;
                quint16 qBitLen = mList.at(j).uLength;
                quint16 paramValue = 0;
                bool bRet = getParamValue16(qRegValue, qPos, qBitLen, paramValue);
                if(bRet)
                {
                    m_signalParamMap[itr.key()].spList[j].uValue = paramValue;
                    //                    QString strkey = mList.at(j).strKey;
                    //                    QString strValue = QString::number(paramValue);
                    //                    emit sig_setPLCMapValue(strkey, strValue);
                }
            }
        }
        else if(iRegBitLengh == 32 || iRegBitLengh == 64)
        {
            if(mList.size() > 0)
            {
                quint64 paramValue = qRegValue;
                m_signalParamMap[itr.key()].spList[0].uValue = paramValue;
            }
        }
    }
}

bool ModBusService::getParamValue16(quint16 regValue, quint16 valuePos, quint16 valueSize, quint16 &paramValue)
{
    if((valuePos + valueSize) > 16)
    {
        return false;
    }
    paramValue = (quint16)((regValue >> valuePos) & (0xFFFF >> (16-valueSize)));
    return true;
}

bool ModBusService::getParamValue32(quint32 regValue, quint16 valuePos, quint16 valueSize, quint32 &paramValue)
{
    if((valuePos + valueSize) > 32)
    {
        return false;
    }
    paramValue = (quint32)((regValue >> valuePos) & (0xFFFFFFFF >> (32-valueSize)));
    return true;
}

bool ModBusService::getParamValue64(quint64 regValue, quint16 valuePos, quint16 valueSize, quint64 &paramValue)
{
    if((valuePos + valueSize) > 64)
    {
        return false;
    }
    paramValue = (quint64)((regValue >> valuePos) & (0xFFFFFFFFFFFFFFFF >> (64-valueSize)));
    return true;
}

bool ModBusService::setParamValue16(quint16 oldRegValue, quint16 valuePos, quint16 valueSize, quint16 setValue, quint16 &newRegValue)
{
    //判断设置的值是否大于最大值，也就是2的valueSize次方
    if(setValue >= pow(2, valueSize))
    {
        return false;
    }
    if((valuePos + valueSize) > 16)
    {
        return false;
    }

    newRegValue = (quint16)((setValue << valuePos) | ( (0xFFFF<<(valuePos+valueSize) | 0xFFFF>>(16-valuePos)) & oldRegValue));
    return true;
}

bool ModBusService::setParamValue32(quint32 oldRegValue, quint16 valuePos, quint16 valueSize, quint32 setValue, quint32 &newRegValue)
{
    //判断设置的值是否大于最大值，也就是2的valueSize次方
    if(setValue >= pow(2, valueSize))
    {
        return false;
    }
    if((valuePos + valueSize) > 32)
    {
        return false;
    }

    newRegValue = (quint32)((setValue << valuePos) | ( (0xFFFFFFFF<<(valuePos+valueSize) | 0xFFFFFFFF>>(32-valuePos)) & oldRegValue));
    return true;
}

bool ModBusService::setParamValue64(quint64 oldRegValue, quint16 valuePos, quint16 valueSize, quint64 setValue, quint64 &newRegValue)
{
    //判断设置的值是否大于最大值，也就是2的valueSize次方
    if(setValue >= pow(2, valueSize))
    {
        return false;
    }
    if((valuePos + valueSize) > 64)
    {
        return false;
    }

    newRegValue = (quint64)((setValue << valuePos) | ( (0xFFFFFFFFFFFFFFFF<<(valuePos+valueSize) | 0xFFFFFFFFFFFFFFFF>>(64-valuePos)) & oldRegValue));
    return true;
}

QVector<quint16> ModBusService::getWriteRegValues(quint16 qRegAddr)
{
    QVector<quint16> regValuesList;
    QMap<quint16, SignalSturct>::iterator itr = m_signalParamMap.find(qRegAddr);
    if(itr != m_signalParamMap.end())
    {
        int iRegBitLengh = itr.value().iRegBitLengh;
        QList<SignalParameter> mList = itr.value().spList;
        if(iRegBitLengh == 16)
        {
            quint16 qRegValue = 0x0;
            for(int j=0; j<mList.size();j++)
            {
                quint16 qPos = mList.at(j).uBitPos;
                quint16 qBitLen = mList.at(j).uLength;
                quint16 setValue = mList.at(j).uValue;
//                if(qRegAddr == 43011)
//                    setValue = QRandomGenerator::global()->generate() % 2;
//                if(qRegAddr == 43031)
//                    setValue = QRandomGenerator::global()->generate() % 65536;
                quint16 qNewValue = 0;
                setParamValue16(qRegValue, qPos, qBitLen, setValue, qNewValue);
                qRegValue  = qNewValue;
            }
            regValuesList.append(qRegValue);
        }
        else if(iRegBitLengh == 32)
        {
            if(mList.size() > 0)
            {
                quint32 setValue = mList.at(0).uValue;
//                if(qRegAddr == 43035)
//                    setValue = QRandomGenerator::global()->generate() % 999999;
                quint16 qNewValueHigh = setValue >> 16;
                quint16 qNewValueLow = setValue;
                regValuesList.append(qNewValueHigh);
                regValuesList.append(qNewValueLow);
            }
        }
        else if(iRegBitLengh == 64)
        {
            if(mList.size() > 0)
            {
                quint64 setValue = mList.at(0).uValue;
//                if(qRegAddr == 43051)
//                    setValue = QRandomGenerator::global()->generate() % 999999999;
                quint16 qNewValueHigh2 = setValue >> 16*3;
                quint16 qNewValueHigh1 = setValue >> 16*2;
                quint16 qNewValueLow2 = setValue >> 16;
                quint16 qNewValueLow1 = setValue;
                regValuesList.append(qNewValueHigh2);
                regValuesList.append(qNewValueHigh1);
                regValuesList.append(qNewValueLow2);
                regValuesList.append(qNewValueLow1);
            }
        }
    }
    return regValuesList;
}

void ModBusService::readRegister2Redis()
{
    //SignalMap到Redis数据库
//    bool isChanged = false;
//    quint16 qRegAddr = m_protocolParam.uReadStartAddr;
//    int qRegCounts = m_protocolParam.iReadRegCounts;
//    int qRegAddrEnd = m_protocolParam.uReadStartAddr+qRegCounts;
//    for( ; qRegAddr < qRegAddrEnd; qRegAddr++)
//    {
//        QList<SignalParameter> mList = m_signalParamMap.value(qRegAddr);
//        for(int j=0; j<mList.size();j++)
//        {
//            QString strKey = mList.at(j).strKey;
//            QString regValue = QString::number(mList.at(j).uValue);
//            QString mapValue = m_readMap.value(strKey);
//            if(!isEqualString(regValue,mapValue))
//            {
//                m_readMap[strKey] = regValue;
//                isChanged = true;
//            }
//        }
//    }

    //值有变化再写入Redis
//    if(isChanged)
//        m_pRedisClient->writeMap2Redis(TOPIC_READ, m_readMap);
}

void ModBusService::redis2WriteRegister()
{
    //读Redis数据到WriteMap
//    m_pRedisClient->readRedis2Map(TOPIC_WRITE, m_writeMap);

    //WriteMap到SignalMap
//    quint16 qRegAddr = m_protocolParam.uWriteStartAddr;
//    int qRegCounts = m_protocolParam.iWriteRegCounts;
//    for( ; qRegAddr < (m_protocolParam.uWriteStartAddr+qRegCounts); qRegAddr++)
//    {
//        QList<SignalParameter> mList = m_signalParamMap.value(qRegAddr);
//        for(int j=0; j<mList.size();j++)
//        {
//            QString strKey = mList.at(j).strKey;
//            QString regValue = QString::number(mList.at(j).uValue);
//            QString mapValue = m_writeMap.value(strKey);
//            if(!isEqualString(regValue,mapValue))
//            {
//                m_signalParamMap[qRegAddr][j].uValue = (quint16)mapValue.toUInt();
//            }
//        }
//    }
//    //WriteMap到寄存器，QModbus只有值改变了才会写入
//    writeRegister();
}

void ModBusService::slot_recvTimeout()
{
    readRegister();
    writeRegister();
    printData();
}

void ModBusService::slot_readReady()
{
    auto reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;

    if (reply->error() == QModbusDevice::NoError)
    {
        const QModbusDataUnit unit = reply->result();
        if(unit.valueCount() == 1)
        {
            quint16 regAddr = unit.startAddress();
            quint16 regValue = unit.value(0);
            quint64 regValueCombine = regValue;
            updateParamValue(regAddr, regValueCombine);
        }
        else if(unit.valueCount() == 2)
        {
            //寄存器到SignalMap
            quint16 regAddr = unit.startAddress();
            quint16 regValueHigh = unit.value(0);
            quint16 regValueLow = unit.value(1);
            quint64 regValueCombine = (static_cast<quint32>(regValueHigh) << 16) | regValueLow;
//            qDebug()<<"regAddr:"<<regAddr<<"regValueHigh:"<<regValueHigh<<"regValueLow:"<<regValueLow<<"regValueCombine"<<regValueCombine;

            updateParamValue(regAddr, regValueCombine);
        }
        else if(unit.valueCount() == 4)
        {
            quint16 regAddr = unit.startAddress();
            quint16 regValueHigh2 = unit.value(0);
            quint16 regValueHigh1 = unit.value(1);
            quint16 regValueLow2 = unit.value(2);
            quint16 regValueLow1 = unit.value(3);
            quint64 regValueCombine =   (static_cast<quint64>(regValueHigh2) << 16*3)
                                      | (static_cast<quint64>(regValueHigh1) << 16*2)
                                      | (static_cast<quint64>(regValueLow2) << 16)
                                      | (static_cast<quint64>(regValueLow1));

            updateParamValue(regAddr, regValueCombine);
        }
    }
    else if (reply->error() == QModbusDevice::ProtocolError)
    {
        qDebug()<<QString("Read response ProtocolError: %1 (Mobus exception: 0x%2)")
                        .arg(reply->errorString())
                        .arg(reply->rawResult().exceptionCode());
    }
    else
    {
        qDebug()<<QString("Read response error: %1 (code: 0x%2)")
                        .arg(reply->errorString())
                        .arg(reply->error());
    }

    reply->deleteLater();
}

void ModBusService::slot_reconnection()
{
    if (!m_modbusDevice)
        return;

    if (m_modbusDevice->state() != QModbusDevice::ConnectedState)
    {
        if (!m_modbusDevice->connectDevice())
        {
            if(!m_reconnectionTimer->isActive())
            {
                qDebug()<<"Connect failed: " + m_modbusDevice->errorString();
                m_reconnectionTimer->start();
                emit sig_setConnected(false);
            }
        }
    }

    if (m_modbusDevice->state() == QModbusDevice::ConnectedState)
    {
        qDebug()<<"Connect success";
        m_reconnectionTimer->stop();
        m_recvTimer->start();
        emit sig_setConnected(true);
    }
}

void ModBusService::initConnection()
{
    QString configPath = qApp->applicationDirPath() + "/config/Config.ini";
    QSettings settings(configPath,QSettings::IniFormat);

    int connectType = settings.value("ConnectType",1).toInt(); //0 Serial 1 TCP
    m_debugType = settings.value("Debug",0).toInt(); //调试类型 0：不输出 1：按寄存器地址输出 2：按每个数据输出

    QString serialPortName = settings.value("Serial/PortName","COM1").toString();
    QString serialParity = settings.value("Serial/Parity","None").toString(); //None Even Odd Space Mark
    int serialBaudRate = settings.value("Serial/BaudRate",115200).toInt(); //1200 2400 4800 9600 19200 38400 57600 115200
    int serialDataBits = settings.value("Serial/DataBits",8).toInt(); //5 6 7 8
    int serialStopBits = settings.value("Serial/StopBits",1).toInt(); // OneStop:1 OneAndHalfStop:3 TwoStop:2
    QString tcpIPPort = settings.value("TCP/IPPort","127.0.0.1:502").toString();
    int timeOut = settings.value("Exception/Timeout",1000).toInt();
    int numberOfRetries = settings.value("Exception/NumberOfRetries",0).toInt();

    int nSerialParity = QSerialPort::NoParity;
    if(serialParity == "Even"){
        nSerialParity = QSerialPort::EvenParity;
    }else if(serialParity == "Odd"){
        nSerialParity = QSerialPort::OddParity;
    }else if(serialParity == "Space"){
        nSerialParity = QSerialPort::SpaceParity;
    }else if(serialParity == "Mark"){
        nSerialParity = QSerialPort::MarkParity;
    }

    //Serial
    if (connectType == 0)
    {
        m_modbusDevice = new QModbusRtuSerialMaster(this);
        m_modbusDevice->setConnectionParameter(QModbusDevice::SerialPortNameParameter,
                                             serialPortName);
        m_modbusDevice->setConnectionParameter(QModbusDevice::SerialParityParameter,
                                             nSerialParity);
        m_modbusDevice->setConnectionParameter(QModbusDevice::SerialBaudRateParameter,
                                             serialBaudRate);
        m_modbusDevice->setConnectionParameter(QModbusDevice::SerialDataBitsParameter,
                                             serialDataBits);
        m_modbusDevice->setConnectionParameter(QModbusDevice::SerialStopBitsParameter,
                                             serialStopBits);
    }
    else //TCP
    {
        m_modbusDevice = new QModbusTcpClient(this);
        const QUrl url = QUrl::fromUserInput(tcpIPPort);
        m_modbusDevice->setConnectionParameter(QModbusDevice::NetworkPortParameter, url.port());
        m_modbusDevice->setConnectionParameter(QModbusDevice::NetworkAddressParameter, url.host());
    }
    m_modbusDevice->setTimeout(timeOut);
    m_modbusDevice->setNumberOfRetries(numberOfRetries);

    connect(m_modbusDevice, &QModbusClient::errorOccurred, [this](QModbusDevice::Error) {
//        qDebug()<<"QModbusDevice::Error"<<m_modbusDevice->errorString();
        m_recvTimer->stop();
        if(!m_reconnectionTimer->isActive())
        {
            qDebug()<<"QModbusDevice::Error"<<m_modbusDevice->errorString();
            m_reconnectionTimer->start();
            emit sig_setConnected(false);
        }
    });
}

void ModBusService::initJsonFile()
{
    QString filePath = qApp->applicationDirPath() + "/config/Protocol.json";
    m_jsonFile.loadJson(filePath);
    m_signalParamMap.clear();
    m_signalParamMap= m_jsonFile.getDataStructMap();
    m_protocolParam.uServerAddr = m_jsonFile.getServerAddress();

    initReadMap();
    initWriteMap();
}

void ModBusService::initReadMap()
{
//    m_readMap.clear();
//    quint16 qRegAddr = m_protocolParam.uReadStartAddr;
//    int qRegCounts = m_protocolParam.iReadRegCounts;

//    for( ; qRegAddr < (m_protocolParam.uReadStartAddr+qRegCounts); qRegAddr++)
//    {
//        QList<SignalParameter> mList = m_signalParamMap.value(qRegAddr);
//        for(int j=0; j<mList.size();j++)
//        {
//            QString strKey = mList.at(j).strKey;
//            QString strValue = QString::number(mList.at(j).uValue);
//            m_readMap.insert(strKey, strValue);
//        }
//    }
}

void ModBusService::initWriteMap()
{
//    m_writeMap.clear();
//    quint16 qRegAddr = m_protocolParam.uWriteStartAddr;
//    int qRegCounts = m_protocolParam.iWriteRegCounts;

//    for( ; qRegAddr < (m_protocolParam.uWriteStartAddr+qRegCounts); qRegAddr++)
//    {
//        QList<SignalParameter> mList = m_signalParamMap.value(qRegAddr);
//        for(int j=0; j<mList.size();j++)
//        {
//            QString strKey = mList.at(j).strKey;
//            QString strValue = QString::number(mList.at(j).uValue);
//            m_writeMap.insert(strKey, strValue);
//        }
//    }
}


bool ModBusService::isEqualString(const QString &str1, const QString &str2)
{
    int ret = QString::compare(str1,str2);
    if(ret == 0)
        return true;
    else
        return false;
}

void ModBusService::printData()
{
    if(m_debugType == 0)
        return;

    if(m_debugType == 1)
    {
        QMap<quint16, SignalSturct>::iterator itr = m_signalParamMap.begin();
        int iCount = 0;
        while(itr != m_signalParamMap.end())
        {
            int iRegBitLengh = itr.value().iRegBitLengh;
            QList<SignalParameter> mList = itr.value().spList;
            quint64 qRegValue64 = 0;
            if(iRegBitLengh == 16)
            {
                quint16 qRegValue = 0x0;
                for(int j=0; j<mList.size();j++)
                {
                    quint16 qPos = mList.at(j).uBitPos;
                    quint16 qBitLen = mList.at(j).uLength;
                    quint16 setValue = mList.at(j).uValue;
                    quint16 qNewValue = 0;
                    setParamValue16(qRegValue, qPos, qBitLen, setValue, qNewValue);
                    qRegValue  = qNewValue;
                }
                qRegValue64 = qRegValue;
            }
            else if(iRegBitLengh == 32 || iRegBitLengh == 64)
            {
                if(mList.size() > 0)
                {
                    qRegValue64 = mList.at(0).uValue;
                }
            }

            if(iCount == 0)
            {
                QString strInfo = QString("==================================================================================================================");
                //                    printf(strInfo.toStdString().c_str());
                qDebug()<<strInfo;
            }

            QString logInfo = QString("%1 %2 %3")
                                  .arg(iCount+1,3)
                                  .arg(itr.key() + REGADDR_OFFSET,10)
                                  .arg(QString::number(qRegValue64,16),10);
            //                printf(logInfo.toStdString().c_str());
            qDebug()<<logInfo;
            iCount++;
            itr++;
        }
    }

    if(m_debugType == 2)
    {
        QMap<quint16, SignalSturct>::iterator itr = m_signalParamMap.begin();
        int iCount = 0;
        while(itr != m_signalParamMap.end())
        {
            QList<SignalParameter> mList = itr.value().spList;
            for(int j=0; j<mList.size();j++)
            {
                QString strKey = mList.at(j).strKey;
                QString strName= mList.at(j).strParamName;
                QString strType = mList.at(j).strType;
                quint16 qBitLen = mList.at(j).uLength;
                quint16 qPos = mList.at(j).uBitPos;
                quint16 qRegisterAddr = mList.at(j).uRegisterAddr + REGADDR_OFFSET;
                quint64 qValue = mList.at(j).uValue;

                if(iCount == 0)
                {
                    QString strInfo = QString("==================================================================================================================");
                    //                    printf(strInfo.toStdString().c_str());
                    qDebug()<<strInfo;
                }
                QString logInfo = QString("%1 %2 %3 %4 %5 %6 %7 %8")
                                      .arg(iCount+1,3)
                                      .arg(strKey,26)
                                      .arg(strType,10)
                                      .arg(qBitLen,10)
                                      .arg(qPos,10)
                                      .arg(qRegisterAddr,10)
                                      .arg(QString::number(qValue,10),10)
                                      .arg(strName,20);
                //                printf(logInfo.toStdString().c_str());

                qDebug()<<logInfo;
                iCount++;
            }
            itr++;
        }
    }
}
