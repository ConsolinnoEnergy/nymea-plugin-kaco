#ifndef KACODISCOVERY_H
#define KACODISCOVERY_H

#include <QHash>
#include <QObject>
#include <QUdpSocket>
#include <QTimer>

class KacoDiscovery : public QObject
{
    Q_OBJECT
public:
    enum RecordType {
        RecordTypeA = 0x0001,
        RecordTypeAAAA = 0x001C,
        RecordTypePtr = 0x000C,
        RecordTypeSrv = 0x0021,
        RecordTypeTxt = 0x0010
    };
    Q_ENUM(RecordType)

    typedef struct KacoDicoveryResult {
        QHostAddress hostAddress;
        quint16 servicePort;
        QString domainName;
        QString serialNumber;
        QString mac;
    } KacoDicoveryResult;


    QList<KacoDicoveryResult> result() const;

    explicit KacoDiscovery(QObject *parent = nullptr);
    ~KacoDiscovery();

public slots:
    void startDiscovery();
    void stopDiscovery();

signals:
    void discoveryFinished();

private:
    QTimer m_discoveryTimer;
    QHash<QHostAddress, KacoDicoveryResult> m_result;
    QHash<int, QUdpSocket *> m_interfaceSockets;

    QHostAddress m_multicastAddress = QHostAddress("224.0.0.251");
    quint16 m_mdnsPort = 5353;
    QString m_searchTarget = "_centurio._tcp";

    bool setMulticastGroup(QUdpSocket *socket, const QHostAddress &groupAddress, const QNetworkInterface &interface, bool join);

    void processDatagram(const QByteArray &datagram);
    QString parseLabel(const QByteArray &datagram);
    QString parseMacAddress(const QByteArray &macAddressBytes);

private slots:
    void discover();
    void readDatagram();
    void onDiscoveryTimeout();

};

#endif // KACODISCOVERY_H
