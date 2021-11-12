#ifndef KACOCLIENT_H
#define KACOCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>

class KacoClient : public QObject
{
    Q_OBJECT
public:
    explicit KacoClient(const QHostAddress &hostAddress, quint16 port = 9760, QObject *parent = nullptr);

    bool connected() const;


public slots:
    void connectToDevice();
    void disconnectFromDevice();

    bool sendData(const QByteArray &data);

signals:
    void connectedChanged(bool connected);

private:
    QTcpSocket *m_socket = nullptr;
    QHostAddress m_hostAddress;
    quint16 m_port;

    QString m_privateKeyBase64 = "MIIBITANBgkqhkiG9w0BAQEFAAOCAQ4AMIIBCQKCAQAws44+vRZyAFtDocnqtHjMQwytJqrjq34t7LuPv+6aC1vuMvvQHTxjeQetsvg1Q3WoK49YfnCrJTRNTX0SCD2SIFVqZqDkJVqCheeZiuk+aCQ3GFpdZdmHkRswaO2s8BqJ0CVTcWCExMbxWDFK/0NIsBdIoykIixv/bwmYRX3WxCG+I3J1Lp9geYu+EPdBy09x0Mbh6rziLfcark9YNUp2Tvj+O2nO1fkSiFOA3czaS042ORXnxRYcl2Zu5DXDb94Uh28JEXWLr02gEqvMCBkx+yYNAT8TRfO2pw8T+CrT+R0tfufL3ELIPGokKQVNvlsYkjEvHQ0M9dYCQqpwAmC7AgMBAAE=";
    QByteArray m_privateKey;
};

#endif // KACOCLIENT_H
