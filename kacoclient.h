/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2021, nymea GmbH
* Contact: contact@nymea.io
*
* This file is part of nymea.
* This project including source code and documentation is protected by
* copyright law, and remains the property of nymea GmbH. All rights, including
* reproduction, publication, editing and translation, are reserved. The use of
* this project is subject to the terms of a license agreement to be concluded
* with nymea GmbH in accordance with the terms of use of nymea GmbH, available
* under https://nymea.io/license
*
* GNU Lesser General Public License Usage
* Alternatively, this project may be redistributed and/or modified under the
* terms of the GNU Lesser General Public License as published by the Free
* Software Foundation; version 3. This project is distributed in the hope that
* it will be useful, but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this project. If not, see <https://www.gnu.org/licenses/>.
*
* For any further details and any questions please contact us under
* contact@nymea.io or see our FAQ/Licensing Information on
* https://nymea.io/license/faq
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef KACOCLIENT_H
#define KACOCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>
#include <QDateTime>
#include <QTimer>

class KacoClient : public QObject
{
    Q_OBJECT
public:
    enum State {
        StateNone,
        StateAuthenticate,
        StateInitialize,
        StateRefreshKey,
        StateRefreshData
    };
    Q_ENUM(State)

    explicit KacoClient(const QHostAddress &hostAddress, quint16 port = 9760, const QString &password = "user", QObject *parent = nullptr);

    bool connected() const;

    QString serialNumber() const;

    // Meter data
    float meterInverterEnergyReturnedToday() const;
    float meterInverterEnergyReturnedMonth() const;
    float meterInverterEnergyReturnedTotal() const;
    float meterInverterEnergyConsumedToday() const;
    float meterInverterEnergyConsumedMonth() const;
    float meterInverterEnergyConsumedTotal() const;
    float meterGridEnergyReturnedToday() const;
    float meterGridEnergyReturnedMonth() const;
    float meterGridEnergyReturnedTotal() const;
    float meterGridEnergyConsumedToday() const;
    float meterGridEnergyConsumedMonth() const;
    float meterGridEnergyConsumedTotal() const;
    float meterSelfConsumptionDay() const;
    float meterSelfConsumptionMonth() const;
    float meterSelfConsumptionTotal() const;
    float meterAhBatteryToday() const;
    float meterAhBatteryMonth() const;
    float meterAhBatteryTotal() const;
    float meterVoltagePhaseA() const;
    float meterVoltagePhaseB() const;
    float meterVoltagePhaseC() const;
    float meterPowerPhaseA() const;
    float meterPowerPhaseB() const;
    float meterPowerPhaseC() const;
    float meterPowerInternalPhaseA() const;
    float meterPowerInternalPhaseB() const;
    float meterPowerInternalPhaseC() const;
    float meterFrequency() const;

    // Inverter data
    float inverterGridVoltagePhaseA() const;
    float inverterGridVoltagePhaseB() const;
    float inverterGridVoltagePhaseC() const;
    float inverterPowerPhaseA() const;
    float inverterPowerPhaseB() const;
    float inverterPowerPhaseC() const;
    float inverterReactivePowerPhaseA() const;
    float inverterReactivePowerPhaseB() const;
    float inverterReactivePowerPhaseC() const;
    float inverterPvVoltage1() const;
    float inverterPvVoltage2() const;
    float inverterPvPower() const;
    float inverterFrequency() const;
    float inverterResistanceIsolation() const;

    // Battery data
    float batteryPower() const;
    float batteryVoltage() const;
    float batteryPercentage() const;

public slots:
    void connectToDevice();
    void disconnectFromDevice();

signals:
    void connectedChanged(bool connected);
    void stateChanged(State state);
    void firmwareVersionChanged(const QString &firmwareVersion);
    void authorizationChanged(bool authorization);

    void serialNumberChanged(const QString &serialNumber);

    void valuesUpdated();

    // Meter signals
    void meterInverterEnergyReturnedTodayChanged(float meterInverterEnergyReturnedToday);
    void meterInverterEnergyReturnedMonthChanged(float meterInverterEnergyReturnedMonth);
    void meterInverterEnergyReturnedTotalChanged(float meterInverterEnergyReturnedTotal);
    void meterInverterEnergyConsumedTodayChanged(float meterInverterEnergyConsumedToday);
    void meterInverterEnergyConsumedMonthChanged(float meterInverterEnergyConsumedMonth);
    void meterInverterEnergyConsumedTotalChanged(float meterInverterEnergyConsumedTotal);
    void meterGridEnergyReturnedTodayChanged(float meterGridEnergyReturnedToday);
    void meterGridEnergyReturnedMonthChanged(float meterGridEnergyReturnedMonth);
    void meterGridEnergyReturnedTotalChanged(float meterGridEnergyReturnedTotal);
    void meterGridEnergyConsumedTodayChanged(float meterGridEnergyConsumedToday);
    void meterGridEnergyConsumedMonthChanged(float meterGridEnergyConsumedMonth);
    void meterGridEnergyConsumedTotalChanged(float meterGridEnergyConsumedTotal);
    void meterSelfConsumptionDayChanged(float meterSelfConsumptionDay);
    void meterSelfConsumptionMonthChanged(float meterSelfConsumptionMonth);
    void meterSelfConsumptionTotalChanged(float meterSelfConsumptionTotal);
    void meterAhBatteryTodayChanged(float meterAhBatteryToday);
    void meterAhBatteryMonthChanged(float meterAhBatteryMonth);
    void meterAhBatteryTotalChanged(float meterAhBatteryTotal);
    void meterVoltagePhaseAChanged(float meterVoltagePhaseA);
    void meterVoltagePhaseBChanged(float meterVoltagePhaseB);
    void meterVoltagePhaseCChanged(float meterVoltagePhaseC);
    void meterPowerPhaseAChanged(float meterPowerPhaseA);
    void meterPowerPhaseBChanged(float meterPowerPhaseB);
    void meterPowerPhaseCChanged(float meterPowerPhaseC);
    void meterPowerInternalPhaseAChanged(float meterPowerInternalPhaseA);
    void meterPowerInternalPhaseBChanged(float meterPowerInternalPhaseB);
    void meterPowerInternalPhaseCChanged(float meterPowerInternalPhaseC);
    void meterFrequencyChanged(float meterFrequency);

    // Inverter signals
    void inverterGridVoltagePhaseAChanged(float inverterGridVoltagePhaseA);
    void inverterGridVoltagePhaseBChanged(float inverterGridVoltagePhaseB);
    void inverterGridVoltagePhaseCChanged(float inverterGridVoltagePhaseC);
    void inverterPowerPhaseAChanged(float inverterPowerPhaseA);
    void inverterPowerPhaseBChanged(float inverterPowerPhaseB);
    void inverterPowerPhaseCChanged(float inverterPowerPhaseC);
    void inverterReactivePowerPhaseAChanged(float inverterReactivePowerPhaseA);
    void inverterReactivePowerPhaseBChanged(float inverterReactivePowerPhaseB);
    void inverterReactivePowerPhaseCChanged(float inverterReactivePowerPhaseC);
    void inverterPvVoltage1Changed(float inverterPvVoltage1);
    void inverterPvVoltage2Changed(float inverterPvVoltage2);
    void inverterPvPowerChanged(float inverterPvPower);
    void inverterFrequencyChanged(float inverterFrequency);
    void inverterResistanceIsolationChanged(float inverterResistanceIsolation);

    // Battery signals
    void batteryPowerChanged(float batteryPower);
    void batteryVoltageChanged(float batteryVoltage);
    void batteryPercentageChanged(float batteryPercentage);

private:
    enum MessageType {
        MessageTypePic = 0x30,
        MessageTypeIds = 0x34,
        MessageTypeIdsResponse = 0x35,
        MessageTypeParams = 0x33
    };

    QTcpSocket *m_socket = nullptr;
    QHostAddress m_hostAddress;
    quint16 m_port = 9760;

    quint8 m_userId = 0;
    QString m_userPassword;
    int m_userPasswordHash = 0;    
    QByteArray m_identKey;

    // Data reading
    QTimer m_refreshTimer;
    bool m_requestPending = false;
    int m_requestPendingTicks = 0;

    // PIC information
    int m_picCounter = 0;
    quint8 m_picHighVersion = 0;
    quint8 m_picLowVersion = 0;
    quint8 m_userType = 0;
    QByteArray m_mac;
    QString m_serialNumber;
    QString m_firmwareVersion;
    QByteArray m_picRandomKey;
    quint32 m_clientId = 0;
    uint m_lastPicTimestamp = 0;
    bool m_communicationVer8x = true;
    bool m_authorization = false;

    // Properties of data sets
    QStringList m_statusProperties;
    QStringList m_demoDataProperties;
    QStringList m_systemInfoProperties;
    QStringList m_inverterProperties;
    QStringList m_vectisProperties;
    QStringList m_batteryProperties;
    QStringList m_dateProperties;
    QStringList m_meterProperties;

    QHash<quint16, QString> m_propertyHashes;

    // Meter information
    float m_meterInverterEnergyReturnedToday = 0;
    float m_meterInverterEnergyReturnedMonth = 0;
    float m_meterInverterEnergyReturnedTotal = 0;
    float m_meterInverterEnergyConsumedToday = 0;
    float m_meterInverterEnergyConsumedMonth = 0;
    float m_meterInverterEnergyConsumedTotal = 0;
    float m_meterGridEnergyReturnedToday = 0;
    float m_meterGridEnergyReturnedMonth = 0;
    float m_meterGridEnergyReturnedTotal = 0;
    float m_meterGridEnergyConsumedToday = 0;
    float m_meterGridEnergyConsumedMonth = 0;
    float m_meterGridEnergyConsumedTotal = 0;
    float m_meterSelfConsumptionDay = 0;
    float m_meterSelfConsumptionMonth = 0;
    float m_meterSelfConsumptionTotal = 0;
    float m_meterAhBatteryToday = 0;
    float m_meterAhBatteryMonth = 0;
    float m_meterAhBatteryTotal = 0;

    // Vectis information
    float m_meterVoltagePhaseA = 0;
    float m_meterVoltagePhaseB = 0;
    float m_meterVoltagePhaseC = 0;
    float m_meterPowerPhaseA = 0;
    float m_meterPowerPhaseB = 0;
    float m_meterPowerPhaseC = 0;
    float m_meterPowerInternalPhaseA = 0;
    float m_meterPowerInternalPhaseB = 0;
    float m_meterPowerInternalPhaseC = 0;
    float m_meterFrequency = 0;

    // Inverter information
    float m_inverterGridVoltagePhaseA = 0;
    float m_inverterGridVoltagePhaseB = 0;
    float m_inverterGridVoltagePhaseC = 0;
    float m_inverterPowerPhaseA = 0;
    float m_inverterPowerPhaseB = 0;
    float m_inverterPowerPhaseC = 0;
    float m_inverterReactivePowerPhaseA = 0;
    float m_inverterReactivePowerPhaseB = 0;
    float m_inverterReactivePowerPhaseC = 0;
    float m_inverterPvVoltage1 = 0;
    float m_inverterPvVoltage2 = 0;
    float m_inverterPvPower = 0;
    float m_inverterFrequency = 0;
    float m_inverterResistanceIsolation = 0;    

    // Battery information
    float m_batteryPower = 0;
    float m_batteryVoltage = 0;
    float m_batteryPercentage = 0;

    // Client state
    State m_state = StateNone;
    void setState(State state);
    void resetData();

    bool sendData(const QByteArray &data);
    void processResponse(const QByteArray &response);

    void sendPicRequest();
    void shuffleBytes(const QByteArray &src, int spos, int len, const QByteArray &key, QByteArray &dest, int dpos, int k);
    QByteArray updateIdentKey(const QByteArray &randomKey);
    void processPicResponse(const QByteArray &message);

    void sendInverterRequest();
    void processInverterResponse(const QByteArray &message);

    QByteArray buildPackage(MessageType messageType, const QByteArray &payload);

    QString byteToHexString(quint8 byte);
    QString byteArrayToHexString(const QByteArray &byteArray);

    QByteArray generateRandomBytes(int count = 10);

    qint32 calculateStringHashCode(const QString &name);
    quint32 calculateChecksum(const QByteArray &data);

    float convertRawValueToFloat(quint32 rawValue);
    QByteArray convertUint32ToByteArrayLittleEndian(quint32 value);
    float convertEnergyToFloat(quint32 rawValue, uint offset, float scale);

    bool picRefreshRequired();
    void printHashCodes(const QStringList &properties);



private slots:
    void refresh();

};

#endif // KACOCLIENT_H
