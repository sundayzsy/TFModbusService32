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

#include "mainwindow.h"
#include "settingsdialog.h"
#include "ui_mainwindow.h"

#include <QModbusRtuSerialSlave>
#include <QModbusTcpServer>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QStatusBar>
#include <QUrl>
#include <QDebug>
#include <QTimer>
#include <math.h>

enum ModbusConnection {
    Serial,
    Tcp
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , modbusDevice(nullptr)
    , m_curCount(0)
{
    ui->setupUi(this);
    setWindowTitle("PLC Data Simulator");
    showMaximized();
    initJsonFile();
    initProcessMap();
    setupWidgetContainers();

    ui->connectType->setCurrentIndex(0);
    on_connectType_currentIndexChanged(0);

    m_settingsDialog = new SettingsDialog(this);
    initActions();

    m_stepTimer = new QTimer(this);
    m_stepTimer->setInterval(4000);
    connect(m_stepTimer, &QTimer::timeout, this, &MainWindow::slot_timeout);
}

MainWindow::~MainWindow()
{
    if (modbusDevice)
        modbusDevice->disconnectDevice();
    delete modbusDevice;

    delete ui;
}

void MainWindow::initActions()
{
    ui->actionConnect->setEnabled(true);
    ui->actionDisconnect->setEnabled(false);
    ui->actionExit->setEnabled(true);
    ui->actionOptions->setEnabled(true);

    connect(ui->actionConnect, &QAction::triggered,
            this, &MainWindow::on_connectButton_clicked);
    connect(ui->actionDisconnect, &QAction::triggered,
            this, &MainWindow::on_connectButton_clicked);

    connect(ui->actionExit, &QAction::triggered, this, &QMainWindow::close);
    connect(ui->actionOptions, &QAction::triggered, m_settingsDialog, &QDialog::show);
}

void MainWindow::on_connectType_currentIndexChanged(int index)
{
    if (modbusDevice) {
        modbusDevice->disconnect();
        delete modbusDevice;
        modbusDevice = nullptr;
    }

    ModbusConnection type = static_cast<ModbusConnection> (index);
    if (type == Serial) {
        modbusDevice = new QModbusRtuSerialSlave(this);
    } else if (type == Tcp) {
        modbusDevice = new QModbusTcpServer(this);
        if (ui->portEdit->text().isEmpty())
            ui->portEdit->setText(QLatin1Literal("127.0.0.1:5020"));
    }
    ui->listenOnlyBox->setEnabled(type == Serial);

    if (!modbusDevice) {
        ui->connectButton->setDisabled(true);
        if (type == Serial)
            statusBar()->showMessage(tr("Could not create Modbus slave."), 5000);
        else
            statusBar()->showMessage(tr("Could not create Modbus server."), 5000);
    } else {
        QModbusDataUnitMap reg;
        reg.insert(QModbusDataUnit::HoldingRegisters, { QModbusDataUnit::HoldingRegisters, 0, 50000});


        modbusDevice->setMap(reg);

        connect(modbusDevice, &QModbusServer::dataWritten,
                this, &MainWindow::updateWidgets);
        connect(modbusDevice, &QModbusServer::stateChanged,
                this, &MainWindow::onStateChanged);
        connect(modbusDevice, &QModbusServer::errorOccurred,
                this, &MainWindow::handleDeviceError);

        connect(ui->listenOnlyBox, &QCheckBox::toggled, this, [this](bool toggled) {
            if (modbusDevice)
                modbusDevice->setValue(QModbusServer::ListenOnlyMode, toggled);
        });
        emit ui->listenOnlyBox->toggled(ui->listenOnlyBox->isChecked());
        connect(ui->setBusyBox, &QCheckBox::toggled, this, [this](bool toggled) {
            if (modbusDevice)
                modbusDevice->setValue(QModbusServer::DeviceBusy, toggled ? 0xffff : 0x0000);
        });
        emit ui->setBusyBox->toggled(ui->setBusyBox->isChecked());

        setupDeviceData();
    }
}

void MainWindow::handleDeviceError(QModbusDevice::Error newError)
{
    if (newError == QModbusDevice::NoError || !modbusDevice)
        return;

    statusBar()->showMessage(modbusDevice->errorString(), 5000);
}

void MainWindow::on_nextBtn_clicked()
{
    QString tmp = getLineEditText("WorkMode");
    if(tmp == "1")
    {
        m_curCount++;
        if(m_curCount >= 0 && m_curCount < m_sampleProcess.size())
        {
            int processNum = m_sampleProcess.at(m_curCount);
            setLineEditText("SampleProcess",QString::number(processNum));
        }
    }
    else if(tmp == "2")
    {
        m_curCount++;
        if(m_curCount >= 0 && m_curCount < m_oldNeedleProcess.size())
        {
            int processNum = m_oldNeedleProcess.at(m_curCount);
            setLineEditText("InsideNeedleProcess",QString::number(processNum));
        }
    }
    else if(tmp == "3")
    {
        m_curCount++;
        if(m_curCount >= 0 && m_curCount < m_newNeedleProcess.size())
        {
            int processNum = m_newNeedleProcess.at(m_curCount);
            setLineEditText("OutsideNeedleProcess",QString::number(processNum));
        }
    }
}

void MainWindow::on_autoBtn_clicked()
{
    if(ui->autoBtn->text() == "自动")
    {
        ui->autoBtn->setText("停止");
        ui->nextBtn->setEnabled(false);
        m_stepTimer->start();
    }
    else
    {
        ui->autoBtn->setText("自动");
        ui->nextBtn->setEnabled(true);
        m_stepTimer->stop();
    }
}

void MainWindow::on_resetBtn_clicked()
{
    m_curCount = 0;
    setLineEditText("WorkMode","0");
    setLineEditText("SampleProcess","0");
    setLineEditText("InsideNeedleProcess","0");
    setLineEditText("OutsideNeedleProcess","130");
}

void MainWindow::slot_timeout()
{
    on_nextBtn_clicked();
}

void MainWindow::on_connectButton_clicked()
{
    bool intendToConnect = (modbusDevice->state() == QModbusDevice::UnconnectedState);

    statusBar()->clearMessage();

    if (intendToConnect) {
        if (static_cast<ModbusConnection> (ui->connectType->currentIndex()) == Serial) {
            modbusDevice->setConnectionParameter(QModbusDevice::SerialPortNameParameter,
                ui->portEdit->text());
            modbusDevice->setConnectionParameter(QModbusDevice::SerialParityParameter,
                m_settingsDialog->settings().parity);
            modbusDevice->setConnectionParameter(QModbusDevice::SerialBaudRateParameter,
                m_settingsDialog->settings().baud);
            modbusDevice->setConnectionParameter(QModbusDevice::SerialDataBitsParameter,
                m_settingsDialog->settings().dataBits);
            modbusDevice->setConnectionParameter(QModbusDevice::SerialStopBitsParameter,
                m_settingsDialog->settings().stopBits);
        } else {
            const QUrl url = QUrl::fromUserInput(ui->portEdit->text());
            modbusDevice->setConnectionParameter(QModbusDevice::NetworkPortParameter, url.port());
            modbusDevice->setConnectionParameter(QModbusDevice::NetworkAddressParameter, url.host());
        }
        modbusDevice->setServerAddress(ui->serverEdit->text().toInt());

        if (!modbusDevice->connectDevice()) {
            statusBar()->showMessage(tr("Connect failed: ") + modbusDevice->errorString(), 5000);
        } else {
            ui->actionConnect->setEnabled(false);
            ui->actionDisconnect->setEnabled(true);
        }
    } else {
        modbusDevice->disconnectDevice();
        ui->actionConnect->setEnabled(true);
        ui->actionDisconnect->setEnabled(false);
    }
}

void MainWindow::onStateChanged(int state)
{
    bool connected = (state != QModbusDevice::UnconnectedState);
    ui->actionConnect->setEnabled(!connected);
    ui->actionDisconnect->setEnabled(connected);

    if (state == QModbusDevice::UnconnectedState)
        ui->connectButton->setText(tr("Connect"));
    else if (state == QModbusDevice::ConnectedState)
        ui->connectButton->setText(tr("Disconnect"));
}

void MainWindow::setRegister(const QString &value)
{
    if (!modbusDevice)
        return;

    const QString objectName = QObject::sender()->objectName();
    QLineEdit *pLineEdit = this->findChild<QLineEdit*>(objectName);
    if(pLineEdit == nullptr)
        return;

    quint16 qPos = pLineEdit->property("BitPos").toInt();
    quint16 qBitLen = pLineEdit->property("Length").toInt();
    quint16 qRegAddr = pLineEdit->property("RegisterAddr").toInt();
    QString strType = pLineEdit->property("Type").toString();

    bool ok = true;
    if(qBitLen == 1 || qBitLen == 16)
    {
        quint16 qSetValue = value.toInt();
        quint16 qRegValue = getDataUnitValue(QModbusDataUnit::HoldingRegisters, qRegAddr);
        quint16 qNewValue = 0;
        setParamValue16(qRegValue, qPos, qBitLen, qSetValue, qNewValue);
        ok = setDataUnitValue(QModbusDataUnit::HoldingRegisters, qRegAddr, qNewValue);
    }
    else if(qBitLen == 32)
    {
        quint32 qSetValue = value.toInt();
        quint16 qNewValueHigh = qSetValue >> 16;
        quint16 qNewValueLow = qSetValue;

        QVector<quint16> valueList;
        valueList.append(qNewValueHigh);
        valueList.append(qNewValueLow);
        ok = setDataUnitValue(QModbusDataUnit::HoldingRegisters, qRegAddr, valueList);
    }
    else if(qBitLen == 64)
    {
        quint64 qSetValue = value.toInt();
        quint16 qNewValueHigh2 = qSetValue >> 16*3;
        quint16 qNewValueHigh1 = qSetValue >> 16*2;
        quint16 qNewValueLow2 = qSetValue >> 16;
        quint16 qNewValueLow1 = qSetValue;

        QVector<quint16> valueList;
        valueList.append(qNewValueHigh2);
        valueList.append(qNewValueHigh1);
        valueList.append(qNewValueLow2);
        valueList.append(qNewValueLow1);
        ok = setDataUnitValue(QModbusDataUnit::HoldingRegisters, qRegAddr, valueList);
    }

    if (!ok)
        statusBar()->showMessage(tr("Could not set register: ") + modbusDevice->errorString(),
                                 5000);

    if(objectName == "WorkMode")
        workModeChanged(value.toInt());
}

void MainWindow::pressText()
{
    if (!modbusDevice)
        return;

    QPushButton *pBtn =  qobject_cast<QPushButton*>(QObject::sender());
    if(pBtn == nullptr)
        return;

    const QString objectName = pBtn->property("LineEditName").toString();
    QLineEdit *pLineEdit = this->findChild<QLineEdit*>(objectName);
    if(pLineEdit == nullptr)
        return;

    pLineEdit->setText("1");
}

void MainWindow::releaseText()
{
    if (!modbusDevice)
        return;

    QPushButton *pBtn =  qobject_cast<QPushButton*>(QObject::sender());
    if(pBtn == nullptr)
        return;

    const QString objectName = pBtn->property("LineEditName").toString();
    QLineEdit *pLineEdit = this->findChild<QLineEdit*>(objectName);
    if(pLineEdit == nullptr)
        return;

    pLineEdit->setText("0");
}

void MainWindow::updateWidgets(QModbusDataUnit::RegisterType table, int address, int size)
{
    switch (table) {

    case QModbusDataUnit::HoldingRegisters:
    {
        if(size == 1)
        {
            quint16 qRegAddr = address;
            quint16 qRegValue = getDataUnitValue(QModbusDataUnit::HoldingRegisters, qRegAddr);

            updateParamValue(qRegAddr, qRegValue);
        }
        else if(size == 2)
        {
            quint16 qRegValueHigh = 0;
            quint16 qRegValueLow = 0;
            quint32 qRegValue = 0;
            quint16 qRegAddr = address;
            qRegValueHigh = getDataUnitValue(QModbusDataUnit::HoldingRegisters, qRegAddr);
            qRegValueLow = getDataUnitValue(QModbusDataUnit::HoldingRegisters, qRegAddr+1);
            qRegValue = (static_cast<quint32>(qRegValueHigh) << 16) | qRegValueLow;
            updateParamValue(qRegAddr, qRegValue);
        }
        else if(size == 4)
        {
            quint16 qRegValueHigh2 = 0;
            quint16 qRegValueHigh1 = 0;
            quint16 qRegValueLow2 = 0;
            quint16 qRegValueLow1 = 0;
            quint64 qRegValue = 0;
            quint16 qRegAddr = address;
            qRegValueHigh2 = getDataUnitValue(QModbusDataUnit::HoldingRegisters, qRegAddr);
            qRegValueHigh1 = getDataUnitValue(QModbusDataUnit::HoldingRegisters, qRegAddr+1);
            qRegValueLow2 = getDataUnitValue(QModbusDataUnit::HoldingRegisters, qRegAddr+2);
            qRegValueLow1 = getDataUnitValue(QModbusDataUnit::HoldingRegisters, qRegAddr+3);
            qRegValue =     (static_cast<quint64>(qRegValueHigh2) << 16*3)
                          | (static_cast<quint64>(qRegValueHigh1) << 16*2)
                          | (static_cast<quint64>(qRegValueLow2) << 16)
                          | (static_cast<quint64>(qRegValueLow1));

            updateParamValue(qRegAddr, qRegValue);
        }
        break;
    }
    default:
        break;
    }
}

// -- private
void MainWindow::setupDeviceData()
{
    if (!modbusDevice)
        return;

    QMap<quint16, SignalSturct>::iterator itr = m_signalParamMap.begin();
    while(itr != m_signalParamMap.end())
    {
        int iRegBitLengh = itr.value().iRegBitLengh;
        quint16 qRegAddr = itr.key(); //偏移地址
        QList<SignalParameter> mList = itr.value().spList;

        if(iRegBitLengh == 16)
        {
            quint16 qRegValue = 0;
            for(int i = 0; i < mList.size(); i++)
            {
                QString strKey = mList.at(i).strKey;
                quint16 qPos = mList.at(i).uBitPos;
                quint16 qBitLen = mList.at(i).uLength;
                quint16 setValue = mList.at(i).uValue;
                setLineEditText(strKey, QString::number(setValue));
            }
        }
        else if(iRegBitLengh  == 32)
        {
            if(mList.size() > 0)
            {
                QString strKey = mList.at(0).strKey;
                quint32 setValue = mList.at(0).uValue;
                setLineEditText(strKey, QString::number(setValue));
            }
        }
        else if(iRegBitLengh == 64)
        {
            if(mList.size() > 0)
            {
                QString strKey = mList.at(0).strKey;
                quint64 setValue = mList.at(0).uValue;
                setLineEditText(strKey, QString::number(setValue));
            }
        }
        itr++;
    }
}

void MainWindow::setupWidgetContainers()
{
    create_UI();
}

void MainWindow::initProcessMap()
{
    //取样流程
    m_sampleProcess.append(0);
    m_sampleProcess.append(1);
    m_sampleProcess.append(2);
    m_sampleProcess.append(3);
    m_sampleProcess.append(4);
    m_sampleProcess.append(5);
    m_sampleProcess.append(11);
    m_sampleProcess.append(30);
    m_sampleProcess.append(40);
    m_sampleProcess.append(41);
    m_sampleProcess.append(50);
    m_sampleProcess.append(60);
    m_sampleProcess.append(70);
    m_sampleProcess.append(90);
    m_sampleProcess.append(100);
    m_sampleProcess.append(121);
    m_sampleProcess.append(122);
    m_sampleProcess.append(130);

    //取旧针流程
    m_oldNeedleProcess.append(0);
    m_oldNeedleProcess.append(1);
    m_oldNeedleProcess.append(2);
    m_oldNeedleProcess.append(3);
    m_oldNeedleProcess.append(4);
    m_oldNeedleProcess.append(5);
    m_oldNeedleProcess.append(10);
    m_oldNeedleProcess.append(11);
    m_oldNeedleProcess.append(12);
    m_oldNeedleProcess.append(14);
    m_oldNeedleProcess.append(20);
    m_oldNeedleProcess.append(21);
    m_oldNeedleProcess.append(22);
    m_oldNeedleProcess.append(30);
    m_oldNeedleProcess.append(31);
    m_oldNeedleProcess.append(32);
    m_oldNeedleProcess.append(40);
    m_oldNeedleProcess.append(41);
    m_oldNeedleProcess.append(42);
    m_oldNeedleProcess.append(43);
    m_oldNeedleProcess.append(44);
    m_oldNeedleProcess.append(45);
    m_oldNeedleProcess.append(46);
    m_oldNeedleProcess.append(47);
    m_oldNeedleProcess.append(50);
    m_oldNeedleProcess.append(51);
    m_oldNeedleProcess.append(52);
    m_oldNeedleProcess.append(53);
    m_oldNeedleProcess.append(118);
    m_oldNeedleProcess.append(120);
    m_oldNeedleProcess.append(124);
    m_oldNeedleProcess.append(125);

    //换新针流程
    m_newNeedleProcess.append(130);
    m_newNeedleProcess.append(132);
    m_newNeedleProcess.append(133);
    m_newNeedleProcess.append(134);
    m_newNeedleProcess.append(135);
    m_newNeedleProcess.append(136);
    m_newNeedleProcess.append(137);
    m_newNeedleProcess.append(138);
    m_newNeedleProcess.append(140);
    m_newNeedleProcess.append(141);
    m_newNeedleProcess.append(142);
    m_newNeedleProcess.append(143);
    m_newNeedleProcess.append(150);
    m_newNeedleProcess.append(151);
    m_newNeedleProcess.append(152);
    m_newNeedleProcess.append(153);
    m_newNeedleProcess.append(154);
    m_newNeedleProcess.append(160);
    m_newNeedleProcess.append(161);
    m_newNeedleProcess.append(162);
    m_newNeedleProcess.append(163);
    m_newNeedleProcess.append(164);
    m_newNeedleProcess.append(170);
    m_newNeedleProcess.append(171);
    m_newNeedleProcess.append(172);
    m_newNeedleProcess.append(173);
    m_newNeedleProcess.append(174);
    m_newNeedleProcess.append(180);
}

void MainWindow::workModeChanged(int workMode)
{
    if(workMode == 3)
    {
        setLineEditText("OutsideNeedleProcess","130");
    }
    else
    {
        setLineEditText("SampleProcess","0");
        setLineEditText("InsideNeedleProcess","0");
    }
}

void MainWindow::initJsonFile()
{
    QString filePath = qApp->applicationDirPath() + "/config/Protocol.json";
    m_jsonFile.loadJson(filePath);
    m_signalParamMap.clear();
    m_signalParamMap= m_jsonFile.getDataStructMap();
    m_protocolParam.uServerAddr = m_jsonFile.getServerAddress();
}

void MainWindow::createUI2VLayout(QVBoxLayout *pVLayout, const SignalParameter &mList)
{
    QString strKey = mList.strKey;
    QString strName = mList.strParamName;
    QString strType = mList.strType;
    quint16 qBitLen = mList.uLength;
    quint16 qPos = mList.uBitPos;
    quint16 qRegAddr = mList.uRegisterAddr;


    QLabel *labelKey = new QLabel(strKey);
    labelKey->setFixedWidth(250);
    QLabel *labelName = new QLabel(strName);
    labelName->setFixedWidth(200);
    QLabel *labelType = new QLabel(strType);
    labelType->setFixedWidth(50);
    QLabel *labelBitLen = new QLabel(QString::number(qBitLen));
    labelBitLen->setFixedWidth(50);
    QLabel *labelPos = new QLabel(QString::number(qPos));
    labelPos->setFixedWidth(50);
    QLabel *labelRegAddr = new QLabel(QString::number(qRegAddr + REGADDR_OFFSET));
    labelRegAddr->setFixedWidth(50);


    QLineEdit *pLineEdit = new QLineEdit();
    pLineEdit->setObjectName(strKey);
    pLineEdit->setProperty("Length",qBitLen);
    pLineEdit->setProperty("BitPos",qPos);
    pLineEdit->setProperty("RegisterAddr",qRegAddr);
    pLineEdit->setProperty("Type",strType);
    pLineEdit->setFixedWidth(100);
    pLineEdit->setText("0");
    //        pLineEdit->setAlignment(Qt::AlignLeft);
    if(strType == "DO" || strType == "AO")
    {
        pLineEdit->setEnabled(false);
    }
    if(strType == "DI" || strType == "DO")
    {
        pLineEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-1]{0,1}"),
                                                                                   QRegularExpression::CaseInsensitiveOption), this));
//                    pLineEdit->setPlaceholderText("Hex 0-1.");
    }
    else if(strType == "AI" || strType == "AO")
    {
        pLineEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9]{0,20}"),
                                                                                   QRegularExpression::CaseInsensitiveOption), this));
        //            pLineEdit->setPlaceholderText("Hex 0-ffff.");
    }
    connect(pLineEdit, &QLineEdit::textChanged, this, &MainWindow::setRegister);

    QPushButton *pBtn = new QPushButton("开关");
    pBtn->setProperty("LineEditName",strKey);
    pBtn->setFixedWidth(40);
    pBtn->hide();
    connect(pBtn, &QPushButton::pressed, this, &MainWindow::pressText);
    connect(pBtn, &QPushButton::released, this, &MainWindow::releaseText);

    QHBoxLayout *pHLayout = new QHBoxLayout();
    pHLayout->addWidget(labelKey);
    pHLayout->addWidget(labelName);
    pHLayout->addWidget(labelType);
    pHLayout->addWidget(labelBitLen);
    pHLayout->addWidget(labelPos);
    pHLayout->addWidget(labelRegAddr);
    pHLayout->addWidget(pLineEdit);
    pHLayout->addWidget(pBtn);
    pVLayout->addLayout(pHLayout);
}

void MainWindow::create_UI()
{
    int iSignalCount = 0;
    int iCount = 0;
    int iTabCount = 0;
    QVBoxLayout *pVLayoutL = nullptr;
    QVBoxLayout *pVLayoutR = nullptr;
    QWidget *pTab = nullptr;
    QHBoxLayout *pRootHLayout = nullptr;

    QMap<quint16, SignalSturct>::iterator itr = m_signalParamMap.begin();
    while(itr != m_signalParamMap.end())
    {
        int iRegBitLengh = itr.value().iRegBitLengh;
        QList<SignalParameter> mList = itr.value().spList;
        for(int i = 0; i < mList.size(); i++)
        {
            iCount++;
            iSignalCount++;
            //左边8行
            if(iCount > 0 && iCount <= 8)
            {
                if(iCount == 1)
                {
                    iTabCount++;
                    pTab = new QWidget();
                    pRootHLayout = new QHBoxLayout(pTab);
                    pRootHLayout->addSpacerItem(new QSpacerItem(0,0,QSizePolicy::Expanding,QSizePolicy::Fixed));
                    pRootHLayout->setContentsMargins(10,0,10,0);
                    ui->tabWidget->addTab(pTab,QString("第%1页").arg(iTabCount));
                    pVLayoutL = new QVBoxLayout();
                    pVLayoutL->setContentsMargins(0,0,60,0);
                }
                createUI2VLayout(pVLayoutL,mList.at(i));
                if(iCount == 8 || iSignalCount == m_jsonFile.getAllSignalCounts())
                {
                    pRootHLayout->addLayout(pVLayoutL);
                }
            }
            //右边8行
            if(iCount > 8 && iCount <= 16)
            {
                if(iCount == 9)
                {
                    pVLayoutR = new QVBoxLayout();
                    pVLayoutR->setContentsMargins(100,0,0,0);
                }
                createUI2VLayout(pVLayoutR,mList.at(i));
                if(iCount == 16 || iSignalCount == m_jsonFile.getAllSignalCounts())
                {
                    pRootHLayout->addLayout(pVLayoutR);
                    iCount = 0;
                }
            }
        }
        itr++;
    }
}

void MainWindow::setLineEditText(const QString &objectName, const QString &value)
{
    QLineEdit *pLineEdt = this->findChild<QLineEdit*>(objectName);
    if(pLineEdt)
    {
        pLineEdt->setText(value);
    }
}

QString MainWindow::getLineEditText(const QString &objectName)
{
    QString str = "";
    QLineEdit *pLineEdt = this->findChild<QLineEdit*>(objectName);
    if(pLineEdt)
    {
        str = pLineEdt->text();
    }
    return str;
}

bool MainWindow::setDataUnitValue(QModbusDataUnit::RegisterType table, quint16 addr, quint16 value)
{
    bool ok = false;
    if (!modbusDevice)
        return ok;

    ok = modbusDevice->setData(table, addr, value);
    return ok;
}

bool MainWindow::setDataUnitValue(QModbusDataUnit::RegisterType table, quint16 addr, const QVector<quint16> &valueList)
{
    bool ok = false;
    QModbusDataUnit unit;
    unit.setRegisterType(table);
    unit.setStartAddress(addr);
    unit.setValues(valueList);
    if (!modbusDevice)
        return ok;

    ok = modbusDevice->setData(unit);
    return ok;
}

quint16 MainWindow::getDataUnitValue(QModbusDataUnit::RegisterType table, quint16 addr)
{
    quint16 value = 0;
    if (!modbusDevice)
        return value;

    modbusDevice->data(table, addr, &value);
    return value;
}

bool MainWindow::getParamValue16(quint16 regValue, quint16 valuePos, quint16 valueSize, quint16 &paramValue)
{
    if((valuePos + valueSize) > 16)
    {
        return false;
    }
    paramValue = (quint16)((regValue >> valuePos) & (0xFFFF >> (16-valueSize)));
    return true;
}

bool MainWindow::getParamValue32(quint32 regValue, quint16 valuePos, quint16 valueSize, quint32 &paramValue)
{
    if((valuePos + valueSize) > 32)
    {
        return false;
    }
    paramValue = (quint32)((regValue >> valuePos) & (0xFFFFFFFF >> (32-valueSize)));
    return true;
}

bool MainWindow::getParamValue64(quint64 regValue, quint16 valuePos, quint16 valueSize, quint64 &paramValue)
{
    if((valuePos + valueSize) > 64)
    {
        return false;
    }
    paramValue = (quint64)((regValue >> valuePos) & (0xFFFFFFFFFFFFFFFF >> (64-valueSize)));
    return true;
}

bool MainWindow::setParamValue16(quint16 oldRegValue, quint16 valuePos, quint16 valueSize, quint16 setValue, quint16 &newRegValue)
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

bool MainWindow::setParamValue32(quint32 oldRegValue, quint16 valuePos, quint16 valueSize, quint16 setValue, quint32 &newRegValue)
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

bool MainWindow::setParamValue64(quint64 oldRegValue, quint16 valuePos, quint16 valueSize, quint64 setValue, quint64 &newRegValue)
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

void MainWindow::updateParamValue(quint16 qRegAddr, quint64 qRegValue)
{
    QMap<quint16, SignalSturct>::iterator itr = m_signalParamMap.find(qRegAddr);
    if(itr != m_signalParamMap.end())
    {
        int iRegBitLengh = itr.value().iRegBitLengh;
        QList<SignalParameter> mList = itr.value().spList;
        if(iRegBitLengh == 16)
        {
            for(int i=0; i < mList.size();i++)
            {
                QString strKey = mList.at(i).strKey;
                quint16 qPos = mList.at(i).uBitPos;
                quint16 qBitLen = mList.at(i).uLength;
                quint16 paramValue = 0;
                bool bRet = getParamValue16(static_cast<quint16>(qRegValue), qPos, qBitLen, paramValue);
                if(bRet)
                {
                    m_signalParamMap[itr.key()].spList[i].uValue = paramValue;
                    setLineEditText(strKey,QString::number(paramValue));
                }
            }
        }
        else if(iRegBitLengh == 32)
        {
            if(mList.size() > 0)
            {
                QString strKey = mList.at(0).strKey;
                m_signalParamMap[itr.key()].spList[0].uValue = static_cast<quint32>(qRegValue);
                setLineEditText(strKey,QString::number(static_cast<quint32>(qRegValue)));
            }
        }
        else if(iRegBitLengh == 64)
        {
            if(mList.size() > 0)
            {
                QString strKey = mList.at(0).strKey;
                m_signalParamMap[itr.key()].spList[0].uValue = static_cast<quint64>(qRegValue);
                setLineEditText(strKey,QString::number(static_cast<quint64>(qRegValue)));
            }
        }
    }
}
