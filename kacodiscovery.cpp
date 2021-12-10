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

#include "kacodiscovery.h"
#include "extern-plugininfo.h"

#include <arpa/inet.h>
#include <net/if.h>
#include <errno.h>

#include <QUrl>
#include <QDataStream>
#include <QNetworkInterface>

KacoDiscovery::KacoDiscovery(QObject *parent) :
    QObject(parent)
{
    m_discoveryTimer.setInterval(1000);
    m_discoveryTimer.setSingleShot(false);
    connect(&m_discoveryTimer, &QTimer::timeout, this, &KacoDiscovery::discover);
}

KacoDiscovery::~KacoDiscovery()
{    
    stopDiscovery();
}

QList<KacoDiscovery::KacoDicoveryResult> KacoDiscovery::result() const
{
    return m_result.values();
}

void KacoDiscovery::startDiscovery()
{
    qCDebug(dcKaco()) << "Discovery: Starting discovery mechanism...";

    // Clean up old discovery results
    m_result.clear();

    // Create a socket for each network interface
    foreach (const QNetworkInterface &interface, QNetworkInterface::allInterfaces()) {
        if (interface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            qCDebug(dcKaco()) << "Discovery: Skipping loop back interface" << interface.name();
            continue;
        }

        if (!interface.flags().testFlag(QNetworkInterface::CanMulticast)) {
            qCDebug(dcKaco()) << "Discovery: Skipping interface" << interface.name() << "because it can not multicast.";
            continue;
        }

        QList<QNetworkAddressEntry> addressEntries = interface.addressEntries();
        if (addressEntries.isEmpty()) {
            qCDebug(dcKaco()) << "Discovery: Skipping interface" << interface.name() << "because no IP configured.";
            continue;
        }

        QNetworkAddressEntry addressEntry = addressEntries.first();

        QHostAddress hostAddress = addressEntry.ip();
        if (hostAddress.protocol() != QAbstractSocket::IPv4Protocol) {
            qCDebug(dcKaco()) << "Discovery: Skipping interface" << interface.name() << "because it is not IPv4.";
            continue;
        }

        qCDebug(dcKaco()) << "Discovery: Creat socket for network interface" << interface.name() << hostAddress.toString();
        QUdpSocket *socket = new QUdpSocket(this);
        if (!socket->bind(m_mdnsPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            qCWarning(dcKaco()) << "Discovery: Failed to bind mDNS socket:" << socket->errorString();
            socket->deleteLater();
            return;
        }

        qCDebug(dcKaco()) << "Discovery: Bound mDNS socket successfully.";
        if (!setMulticastGroup(socket, m_multicastAddress, interface, true)) {
            qCWarning(dcKaco()) << "Discovery: Failed to join multicast on interface" << interface.name() << socket->errorString();
            socket->deleteLater();
            continue;
        }

        connect(socket, &QUdpSocket::readyRead, this, &KacoDiscovery::readDatagram);
        m_interfaceSockets.insert(interface.index(), socket);
    }

    // Discover for 10 seconds
    QTimer::singleShot(10000, this, &KacoDiscovery::onDiscoveryTimeout);
    m_discoveryTimer.start();
}

void KacoDiscovery::stopDiscovery()
{
    m_discoveryTimer.stop();

    qCDebug(dcKaco()) << "Discovery: Stopping discovery mechanism...";
    foreach (QUdpSocket *socket, m_interfaceSockets) {
        QNetworkInterface interface = QNetworkInterface::interfaceFromIndex(m_interfaceSockets.key(socket));
        if (!setMulticastGroup(socket, m_multicastAddress, interface, false)) {
            qCWarning(dcKaco()) << "Discovery: Failed to leave multicast group" << m_multicastAddress.toString();
        }
        m_interfaceSockets.remove(m_interfaceSockets.key(socket));
        socket->deleteLater();
    }
}

bool KacoDiscovery::setMulticastGroup(QUdpSocket *socket, const QHostAddress &groupAddress, const QNetworkInterface &interface, bool join)
{
    int sockOpt = 0;
    void *sockArg;
    int sockArgSize;
    ip_mreq mreq4;
    ipv6_mreq mreq6;

    if (groupAddress.protocol() == QAbstractSocket::IPv6Protocol) {
        if(join) {
            sockOpt = IPV6_JOIN_GROUP;
        } else {
            sockOpt = IPV6_LEAVE_GROUP;
        }
        sockArg = &mreq6;
        sockArgSize = sizeof(mreq6);
        memset(&mreq6, 0, sizeof(mreq6));
        Q_IPV6ADDR ip6 = groupAddress.toIPv6Address();
        memcpy(&mreq6.ipv6mr_multiaddr, &ip6, sizeof(ip6));
        mreq6.ipv6mr_interface = interface.index();
    } else if (groupAddress.protocol() == QAbstractSocket::IPv4Protocol) {
        if (join) {
            sockOpt = IP_ADD_MEMBERSHIP;
        } else {
            sockOpt = IP_DROP_MEMBERSHIP;
        }
        sockArg = &mreq4;
        sockArgSize = sizeof(mreq4);
        memset(&mreq4, 0, sizeof(mreq4));
        mreq4.imr_multiaddr.s_addr = htonl(groupAddress.toIPv4Address());
        if (interface.isValid()) {
            QList<QNetworkAddressEntry> addressEntries = interface.addressEntries();
            if (!addressEntries.isEmpty()) {
                QHostAddress firstIP = addressEntries.first().ip();
                mreq4.imr_interface.s_addr = htonl(firstIP.toIPv4Address());
            } else {
                qCWarning(dcKaco()) << "Discovery: multicast: interface has no entries";
                return false;
            }
        } else {
            mreq4.imr_interface.s_addr = INADDR_ANY;
        }
    } else {
        return false;
    }

    int res = setsockopt(socket->socketDescriptor(), IPPROTO_IP, sockOpt, sockArg, sockArgSize);
    if (res != 0) {
        qCDebug(dcKaco()) << "Discovery: Error setting sockopt:" << strerror(errno);
        return false;
    }
    return true;
}

void KacoDiscovery::processDatagram(const QByteArray &datagram)
{
    QDataStream stream(datagram);
    quint16 transactionId;
    quint16 flagType;
    stream >> transactionId >> flagType;
    // We search for Standard Query response
    if (flagType != 0x8400) {
        //qCDebug(dcKaco()) << "Discovery: Not a standard query response. Skipping packet...";
        return;
    }

    quint16 question;
    quint16 answerCount;
    quint16 authorityCount;
    quint16 additionalCount;
    stream >> question >> answerCount >> authorityCount >> additionalCount;
    qCDebug(dcKaco()) << "Discovery: #############################################";
    qCDebug(dcKaco()) << "Discovery: Questions:" << question;
    qCDebug(dcKaco()) << "Discovery: Answer RRs:" << answerCount;
    qCDebug(dcKaco()) << "Discovery: Authority RRs:" << authorityCount;
    qCDebug(dcKaco()) << "Discovery: Additional RRs:" << additionalCount;
    if (answerCount == 0) {
        qCDebug(dcKaco()) << "Discovery: No answers to parse. Discarding packet...";
        return;
    }

    // Data we are interested in:
    QHostAddress hostAddress; // Get it from the A record
    quint16 servicePort = 0; // Get it from the SRV record
    QString serialNumber;
    QString domainName;
    QString mac;
    bool discoveredKaco = false;

    // Parse answers
    for (int i = 0; i < answerCount; ++i) {
        // Read the name until \0
        QByteArray nameData;
        while (!stream.atEnd()) {
            quint8 nameDataByte;
            stream >> nameDataByte;
            nameData.append(nameDataByte);
            if (nameDataByte == 0) {
                break;
            }
        }

        QString name = parseLabel(nameData);
        quint16 type; quint16 classFlag;
        stream >> type >> classFlag;

        quint32 ttl; quint16 dataLength;
        stream  >> ttl >> dataLength;

        if (name.contains(m_searchTarget)) {
            // We found a kaco we where searching for.
            discoveredKaco = true;
        }

        QByteArray answerData;
        for (int i = 0; i < dataLength && !stream.atEnd(); i++) {
            quint8 answerByte;
            stream >> answerByte;
            answerData.append(answerByte);
        }

        if (answerData.count() == 0)
            continue;

        //qCDebug(dcKaco()) << "Discovery: Answer data length" << answerData.length() << answerData.toHex();

        // Now parse the answer data depending on the type
        switch (type) {
        case RecordTypeA: {
            qCDebug(dcKaco()) << "Discovery: --> Answer" << i << "| Name:" << name << "TTL:" << ttl << "Length:" << dataLength << static_cast<RecordType>(type);
            QDataStream answerDataStream(&answerData, QIODevice::ReadOnly);
            if (dataLength == 4) {
                quint32 hostAddressRaw;
                answerDataStream >> hostAddressRaw;
                hostAddress = QHostAddress(hostAddressRaw);
                qCDebug(dcKaco()) << "Discovery:     A:" << hostAddress.toString();
                domainName = name;
            } else if (dataLength == 8) {
                quint64 hostAddressRaw;
                answerDataStream >> hostAddressRaw;
                hostAddress = QHostAddress(hostAddressRaw);
                qCDebug(dcKaco()) << "Discovery:     A:" << hostAddress.toString();
            }
            break;
        }
        case RecordTypeSrv: { // SRV record (Server selection)
            qCDebug(dcKaco()) << "Discovery: --> Answer" << i << "| Name:" << name << "TTL:" << ttl << "Length:" << dataLength;
            QDataStream answerDataStream(&answerData, QIODevice::ReadOnly);
            quint16 priority; quint16 weight;
            answerDataStream >> priority >> weight >> servicePort;
            QByteArray targetData;
            while (!answerDataStream.atEnd()) {
                quint8 targetByte;
                answerDataStream >> targetByte;
                targetData.append(targetByte);
            }

            QString target = parseLabel(targetData);
            qCDebug(dcKaco()) << "Discovery:     SRV: Service port:" << servicePort << "target:" << target;
            break;
        }
        case RecordTypePtr: { // PTR record
            qCDebug(dcKaco()) << "Discovery: --> Answer" << i << "| Name:" << name << "TTL:" << ttl << "Length:" << dataLength;
            QDataStream answerDataStream(&answerData, QIODevice::ReadOnly);
            QByteArray domainData;
            while (!answerDataStream.atEnd()) {
                quint8 domainByte;
                answerDataStream >> domainByte;
                domainData.append(domainByte);
                if (domainByte == 0) {
                    break;
                }
            }

            qCDebug(dcKaco()) << "Discovery:     PTR: Domain name:" << parseLabel(domainData);
            break;
        }
        case RecordTypeTxt: {
            qCDebug(dcKaco()) << "Discovery: --> Answer" << i << "| Name:" << name << "TTL:" << ttl << "Length:" << dataLength;
            QDataStream answerDataStream(&answerData, QIODevice::ReadOnly);
            quint8 txtLength;
            QStringList txtRecords;
            while (!answerDataStream.atEnd()) {
                answerDataStream >> txtLength;
                QByteArray txtData;
                for (int i = 0; i < txtLength; i++) {
                    quint8 targetByte;
                    answerDataStream >> targetByte;
                    txtData.append(targetByte);
                }
                QString record = QString::fromUtf8(txtData).trimmed();
                txtRecords.append(record);
                qCDebug(dcKaco()) << "Discovery:     TXT:" << record;
            }

            if (discoveredKaco && txtRecords.count() == 1) {
                QStringList tokes = name.split('.');
                if (tokes.count() >= 1) {
                    QString macString = tokes.first();
                    if (macString.count() == 12) {
                        mac = parseMacAddress(QByteArray::fromHex(macString.toUtf8()));
                    }

                }
                serialNumber = txtRecords.first();
            }

            break;
        }
        default:
            // We don't care about the rest
            break;
        }
    }

    if (discoveredKaco && !m_result.contains(hostAddress)) {
        qCDebug(dcKaco()) << "Discovery: === Found Kaco Inverter ===";
        qCDebug(dcKaco()) << "Discovery: - Host address:" << hostAddress.toString();
        qCDebug(dcKaco()) << "Discovery: - Domain:" << domainName;
        qCDebug(dcKaco()) << "Discovery: - Serial number:" << serialNumber;
        qCDebug(dcKaco()) << "Discovery: - Service port:" << servicePort;
        qCDebug(dcKaco()) << "Discovery: - MAC:" << mac;

        KacoDicoveryResult result;
        result.hostAddress = hostAddress;
        result.servicePort = servicePort;
        result.domainName = domainName;
        result.serialNumber = serialNumber;
        result.mac = mac;
        m_result.insert(hostAddress, result);
    }
}

QString KacoDiscovery::parseLabel(const QByteArray &datagram)
{
    QString label;
    QDataStream stream(datagram);
    quint8 length;

    QStringList sections;
    while (!stream.atEnd()) {
        stream >> length;
        if (length == 0)
            break;

        QByteArray section;
        for (int i = 0; i < length; i++) {
            quint8 byte;
            stream >> byte;
            section.append(byte);
        }

        sections.append(QString::fromUtf8(section));
    }

    label = sections.join('.');
    //qCDebug(dcKaco()) << "Parsed label" << label << "from" << datagram.toHex();
    return label;
}

QString KacoDiscovery::parseMacAddress(const QByteArray &macAddressBytes)
{
    QStringList tokens;
    for (int i = 0; i < macAddressBytes.count(); i++) {
        tokens.append(QString("%1").arg(macAddressBytes.at(i), 2, 16, QLatin1Char('0')));
    }
    return tokens.join(":");
}


void KacoDiscovery::discover()
{
    foreach (QUdpSocket *socket, m_interfaceSockets) {
        QNetworkInterface interface = QNetworkInterface::interfaceFromIndex(m_interfaceSockets.key(socket));
        //qCDebug(dcKaco()) << "Discovery: sending query request to intrface" << interface.name();
        // Note: they send always the same static discovery message
        //    // DNS-SD Header
        //    0x00, 0x00, // Transaction ID
        //    0x00, 0x00, // Flags (Standard Query)
        //    0x00, 0x01, // Questions
        //    0x00, 0x00, // Answer PRs
        //    0x00, 0x00, // Authority PRs
        //    0x00, 0x00, // Additional PRs
        //    // Query
        //    0x09, // Length
        //    0x5f, 0x63, 0x65, 0x6e, 0x74, 0x75, 0x72, 0x69, 0x6f, // _centurio
        //    0x04, // Length
        //    0x5f, 0x74, 0x63, 0x70, // _tcp
        //    0x05, // Length
        //    0x6c, 0x6f, 0x63, 0x61, 0x6c, // local
        //    0x00, // \0
        //    0x00, 0x0c, // Type PTR (Domain name pointer)
        //    0x00, 0x01  // Class IN
        //};

        int bytesWritte = socket->writeDatagram(QByteArray::fromHex("000000000001000000000000095f63656e747572696f045f746370056c6f63616c00000c0001"), m_multicastAddress, m_mdnsPort);
        if (bytesWritte < 0) {
            qCWarning(dcKaco()) << "Discovery: failed to send discovery requet to mDNS multicast" << interface.name() << m_multicastAddress.toString() << m_mdnsPort;
        }
    }
}

void KacoDiscovery::readDatagram()
{
    QUdpSocket *socket = qobject_cast<QUdpSocket *>(sender());
    while (socket->hasPendingDatagrams()) {
        qint64 size = socket->pendingDatagramSize();
        //qCDebug(dcKaco()) << "Discovery: Received datagram ( size" << size << ")";
        if (size == 0)
            return;

        QByteArray datagram;
        datagram.resize(size);
        socket->readDatagram(datagram.data(), size);

        processDatagram(datagram);
    }
}

void KacoDiscovery::onDiscoveryTimeout()
{
    stopDiscovery();
    emit discoveryFinished();
}
