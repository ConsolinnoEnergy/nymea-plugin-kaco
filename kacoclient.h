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
    float meterInverterEnergyReturnedPhaseA() const;
    float meterInverterEnergyReturnedPhaseB() const;
    float meterInverterEnergyReturnedPhaseC() const;
    float meterInverterEnergyConsumedPhaseA() const;
    float meterInverterEnergyConsumedPhaseB() const;
    float meterInverterEnergyConsumedPhaseC() const;
    float meterGridEnergyReturnedPhaseA() const;
    float meterGridEnergyReturnedPhaseB() const;
    float meterGridEnergyReturnedPhaseC() const;
    float meterGridEnergyConsumedPhaseA() const;
    float meterGridEnergyConsumedPhaseB() const;
    float meterGridEnergyConsumedPhaseC() const;
    float meterSelfConsumptionPhaseA() const;
    float meterSelfConsumptionPhaseB() const;
    float meterSelfConsumptionPhaseC() const;
    float meterAhBatteryPhaseA() const;
    float meterAhBatteryPhaseB() const;
    float meterAhBatteryPhaseC() const;
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

    void serialNumberChanged(const QString &serialNumber);

    void valuesUpdated();

    // Meter signals
    void meterInverterEnergyReturnedPhaseAChanged(float meterInverterEnergyReturnedPhaseA);
    void meterInverterEnergyReturnedPhaseBChanged(float meterInverterEnergyReturnedPhaseB);
    void meterInverterEnergyReturnedPhaseCChanged(float meterInverterEnergyReturnedPhaseC);
    void meterInverterEnergyConsumedPhaseAChanged(float meterInverterEnergyConsumedPhaseA);
    void meterInverterEnergyConsumedPhaseBChanged(float meterInverterEnergyConsumedPhaseB);
    void meterInverterEnergyConsumedPhaseCChanged(float meterInverterEnergyConsumedPhaseC);
    void meterGridEnergyReturnedPhaseAChanged(float meterGridEnergyReturnedPhaseA);
    void meterGridEnergyReturnedPhaseBChanged(float meterGridEnergyReturnedPhaseB);
    void meterGridEnergyReturnedPhaseCChanged(float meterGridEnergyReturnedPhaseC);
    void meterGridEnergyConsumedPhaseAChanged(float meterGridEnergyConsumedPhaseA);
    void meterGridEnergyConsumedPhaseBChanged(float meterGridEnergyConsumedPhaseB);
    void meterGridEnergyConsumedPhaseCChanged(float meterGridEnergyConsumedPhaseC);
    void meterSelfConsumptionPhaseAChanged(float meterSelfConsumptionPhaseA);
    void meterSelfConsumptionPhaseBChanged(float meterSelfConsumptionPhaseB);
    void meterSelfConsumptionPhaseCChanged(float meterSelfConsumptionPhaseC);
    void meterAhBatteryPhaseAChanged(float meterAhBatteryPhaseA);
    void meterAhBatteryPhaseBChanged(float meterAhBatteryPhaseB);
    void meterAhBatteryPhaseCChanged(float meterAhBatteryPhaseC);
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
    QByteArray m_picRandomKey;
    quint32 m_clientId = 0;
    uint m_lastPicTimestamp = 0;

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
    float m_meterInverterEnergyReturnedPhaseA = 0;
    float m_meterInverterEnergyReturnedPhaseB = 0;
    float m_meterInverterEnergyReturnedPhaseC = 0;
    float m_meterInverterEnergyConsumedPhaseA = 0;
    float m_meterInverterEnergyConsumedPhaseB = 0;
    float m_meterInverterEnergyConsumedPhaseC = 0;
    float m_meterGridEnergyReturnedPhaseA = 0;
    float m_meterGridEnergyReturnedPhaseB = 0;
    float m_meterGridEnergyReturnedPhaseC = 0;
    float m_meterGridEnergyConsumedPhaseA = 0;
    float m_meterGridEnergyConsumedPhaseB = 0;
    float m_meterGridEnergyConsumedPhaseC = 0;
    float m_meterSelfConsumptionPhaseA = 0;
    float m_meterSelfConsumptionPhaseB = 0;
    float m_meterSelfConsumptionPhaseC = 0;
    float m_meterAhBatteryPhaseA = 0;
    float m_meterAhBatteryPhaseB = 0;
    float m_meterAhBatteryPhaseC = 0;

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
