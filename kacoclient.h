#ifndef KACOCLIENT_H
#define KACOCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>
#include <QTimer>

class KacoClient : public QObject
{
    Q_OBJECT
public:
    enum State {
        StateInit,
        StateRequestPic,
        StateRequestInverter
    };
    Q_ENUM(State)

    explicit KacoClient(const QHostAddress &hostAddress, quint16 port = 9760, QObject *parent = nullptr);

    bool connected() const;

    float gridVoltagePhaseA() const;
    float gridVoltagePhaseB() const;
    float gridVoltagePhaseC() const;

public slots:
    void connectToDevice();
    void disconnectFromDevice();

signals:
    void connectedChanged(bool connected);

    void gridVoltagePhaseAChanged(float gridVoltagePhaseA);
    void gridVoltagePhaseBChanged(float gridVoltagePhaseB);
    void gridVoltagePhaseCChanged(float gridVoltagePhaseC);

private:
    enum MessageType {
        MessageTypePic = 0x30,
        MessageTypeIds = 0x34,
        MessageTypeIdsResponse = 0x35,
        MessageTypeParams = 0x33
    };

    QTcpSocket *m_socket = nullptr;
    QHostAddress m_hostAddress;
    quint16 m_port;
    quint8 m_userId = 0;
    quint32 m_userKey = 1965780472;

    // Data reading
    QTimer m_refreshTimer;
    bool m_requestPending = false;
    int m_requestPendingTicks = 0;

    // Inverter information
    float m_gridVoltagePhaseA = 0;
    float m_gridVoltagePhaseB = 0;
    float m_gridVoltagePhaseC = 0;

    QString m_privateKeyBase64 = "MIIBITANBgkqhkiG9w0BAQEFAAOCAQ4AMIIBCQKCAQAws44+vRZyAFtDocnqtHjMQwytJqrjq34t7LuPv+6aC1vuMvvQHTxjeQetsvg1Q3WoK49YfnCrJTRNTX0SCD2SIFVqZqDkJVqCheeZiuk+aCQ3GFpdZdmHkRswaO2s8BqJ0CVTcWCExMbxWDFK/0NIsBdIoykIixv/bwmYRX3WxCG+I3J1Lp9geYu+EPdBy09x0Mbh6rziLfcark9YNUp2Tvj+O2nO1fkSiFOA3czaS042ORXnxRYcl2Zu5DXDb94Uh28JEXWLr02gEqvMCBkx+yYNAT8TRfO2pw8T+CrT+R0tfufL3ELIPGokKQVNvlsYkjEvHQ0M9dYCQqpwAmC7AgMBAAE=";
    QByteArray m_privateKey;

    State m_state = StateInit;
    QByteArray createMessagePic();

    // Requests
    bool sendData(const QByteArray &data);

    void sendPicRequest();
    void sendInverterRequest();

    // Response
    void processResponse(const QByteArray &response);

    void processPicResponse(const QByteArray &message);
    void processInverterResponse(const QByteArray &message);

    void processIds(const QByteArray &message);

    QString byteToHexString(quint8 byte);
    QString byteArrayToHexString(const QByteArray &byteArray);

    QByteArray generateRandomBytes(int count = 6);

    quint16 getStringHash(const QString &name);
    quint32 calculateChecksum(const QByteArray &data);

    float convertRawValueToFloat(quint32 rawValue);

private slots:
    void refresh();

};

#endif // KACOCLIENT_H
