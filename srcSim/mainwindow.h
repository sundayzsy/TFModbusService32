/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the QtSerialBus module.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QButtonGroup>
#include <QMainWindow>
#include <QModbusServer>
#include <QVBoxLayout>
#include "protocoljson.h"
#include "commondefine.h"

QT_BEGIN_NAMESPACE

class QLineEdit;

namespace Ui {
class MainWindow;
class SettingsDialog;
}

QT_END_NAMESPACE

class SettingsDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private Q_SLOTS:
    void on_connectButton_clicked();
    void onStateChanged(int state);

    void setRegister(const QString &value);
    void pressText();
    void releaseText();
    void updateWidgets(QModbusDataUnit::RegisterType table, int address, int size);

    void on_connectType_currentIndexChanged(int);
    void handleDeviceError(QModbusDevice::Error newError);

    void on_nextBtn_clicked();
    void on_autoBtn_clicked();
    void on_resetBtn_clicked();
    void slot_timeout();

private:
    void initActions();
    void setupDeviceData();
    void setupWidgetContainers();
    void initProcessMap();
    void workModeChanged(int workMode);

    void initJsonFile();
    void createUI2VLayout(QVBoxLayout *pVLayout, const SignalParameter &mList);
    void create_UI();

    void setLineEditText(const QString &objectName, const QString &value);
    QString getLineEditText(const QString &objectName);

    bool setDataUnitValue(QModbusDataUnit::RegisterType table, quint16 addr, quint16 value);
    bool setDataUnitValue(QModbusDataUnit::RegisterType table, quint16 addr, const QVector<quint16> &valueList);
    quint16 getDataUnitValue(QModbusDataUnit::RegisterType table, quint16 addr);

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
    bool setParamValue32(quint32 oldRegValue, quint16 valuePos, quint16 valueSize, quint16 setValue, quint32 &newRegValue);
    bool setParamValue64(quint64 oldRegValue, quint16 valuePos, quint16 valueSize, quint64 setValue, quint64 &newRegValue);

    //qRegAddr：寄存器地址  qRegValue：寄存器值
    void updateParamValue(quint16 qRegAddr, quint64 qRegValue);

private:

    Ui::MainWindow *ui;
    QModbusServer *modbusDevice;
    SettingsDialog *m_settingsDialog;
    ProtocolJson m_jsonFile;

    //保存参数Map Key:寄存器地址 QList<SignalParameter>寄存器下对应的参数列表
    QMap<quint16, SignalSturct> m_signalParamMap;
    SignalProtocolParam m_protocolParam;  //协议参数

    QList<int> m_sampleProcess;   //取样流程
    QList<int> m_oldNeedleProcess;//取旧针流程
    QList<int> m_newNeedleProcess;//换新针流程
    int m_curCount;

    QTimer *m_stepTimer;
};

#endif // MAINWINDOW_H
