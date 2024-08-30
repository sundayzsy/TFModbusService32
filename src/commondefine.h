#ifndef COMMONDEFINE_H
#define COMMONDEFINE_H
#include <QString>

//软件寄存器地址比设备低1，设备400地址，软件要读399
//#define REGADDR_OFFSET 1
#define REGADDR_OFFSET 0

//通讯协议参数
struct  SignalProtocolParam
{
    int iReadRegCounts;                  //读寄存器个数
    int iWriteRegCounts;                 //写寄存器个数
    quint16 uServerAddr;                 //服务器地址
    quint16 uReadStartAddr;              //读寄存器开始地址
    quint16 uWriteStartAddr;              //写寄存器开始地址
    quint8  uFunctionCode;               //功能码 0x17
};

struct SignalParameter
{
    QString  strKey;                     //Key值
    QString  strParamName;               //参数名称
    QString  strType;                    //参数类型
    QString  strDesc;                    //参数描述
    quint16  uRegisterAddr;              //寄存器地址
    quint16  uBitPos;                    //BIT位偏移
    quint16  uLength;                    //数据BIT位长度
    quint64  uValue;                     //参数数值
};

struct SignalSturct
{
    int iRegBitLengh;               //寄存器占用长度 1位、16位为16，32位为32，64位为64
    bool bIsReadReg;                //读寄存器还是写寄存器
    QList<SignalParameter> spList;
};

#endif // COMMONDEFINE_H
