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
