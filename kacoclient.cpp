#include "kacoclient.h"
#include "extern-plugininfo.h"

#include <QDataStream>

KacoClient::KacoClient(const QHostAddress &hostAddress, quint16 port, QObject *parent) :
    QObject(parent),
    m_hostAddress(hostAddress),
    m_port(port)
{
    //qCDebug(dcKaco()) << m_privateKeyBase64;
    //m_privateKey = QByteArray::fromBase64(m_privateKeyBase64.toUtf8());
    //    qCDebug(dcKaco()) << m_privateKey;
    //    qCDebug(dcKaco()) << m_privateKey.toHex();
    //    qCDebug(dcKaco()) << QString::fromUtf8(m_privateKey);

//    QByteArray picMessage = createMessagePic();
//    qCDebug(dcKaco()) << byteArrayToHexString(picMessage);

//    QString hashTestString("g_sync.u_l_rms[0]");
//    qCDebug(dcKaco()) << "Hash test" << 0x7848 << hashTestString << qHash(hashTestString) << getStringHash(hashTestString);


//    // Request: 55aa34100084090000487867788678bccadbca539694d86afd
//    QByteArray response = QByteArray::fromHex("edde354800fc1a000004004878c7746a4304006778f8866c4304008678d3d16a430400bcca052928440400dbca87e60d4404005396bbfc0245040094d89a9953430c006afd80786843801b6b4348ea6a43");
//    processIds(response);



    // Refresh timer
    m_refreshTimer.setInterval(1000);
    m_refreshTimer.setSingleShot(false);
    connect(&m_refreshTimer, &QTimer::timeout, this, &KacoClient::refresh);

    // TCP socket
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, [=](){
        qCDebug(dcKaco()) << "Connected to" << QString("%1:%2").arg(m_hostAddress.toString()).arg(m_port);
        emit connectedChanged(true);
        m_state = StateRequestPic;
        qCDebug(dcKaco()) << m_state;
        refresh();
        m_refreshTimer.start();
    });

    connect(m_socket, &QTcpSocket::disconnected, this, [=](){
        qCDebug(dcKaco()) << "Disconnected from" << QString("%1:%2").arg(m_hostAddress.toString()).arg(m_port) << m_socket->errorString();
        m_state = StateInit;
        qCDebug(dcKaco()) << m_state;
        emit connectedChanged(false);
        QTimer::singleShot(2000, this, &KacoClient::connectToDevice);
        m_refreshTimer.stop();
    });

    connect(m_socket, &QTcpSocket::readyRead, this, [=](){
        QByteArray data = m_socket->readAll();
        qCDebug(dcKaco()) << "Data from" << QString("%1:%2").arg(m_hostAddress.toString()).arg(m_port);
        qCDebug(dcKaco()) << "<--" << qUtf8Printable(data.toHex());
        processResponse(data);
    });

    connect(m_socket, &QTcpSocket::stateChanged, this, [=](QAbstractSocket::SocketState socketState){
        qCDebug(dcKaco()) << "Socket state changed" << QString("%1:%2").arg(m_hostAddress.toString()).arg(m_port) << socketState;
    });
}

bool KacoClient::connected() const
{
    return (m_socket->state() == QAbstractSocket::ConnectedState);
}

bool KacoClient::sendData(const QByteArray &data)
{
    if (!connected())
        return false;

    qCDebug(dcKaco()) << "-->" << qUtf8Printable(data.toHex());
    int writtenBytes = m_socket->write(data);
    return (writtenBytes == data.count());
}

void KacoClient::sendPicRequest()
{
    // Gets sent 4 times:

    // 55aa 30 0b00 ad030000 1b3d165f273c baa259c8 00
    // edde303900e00b00000200234b050906001ab168271939269c02001dcf0102140025a13230313231303234353537382020202020202020bfb3ae2fb0bf0000000000

    // 55aa 30 0b00 7e040000 991f3a2db9e0 0040e4a1 01
    // edde303900650c00000200234b050906001ab168271939269c02001dcf0102140025a13230313231303234353537382020202020202020dae8f07880960200000100

    // 55aa 30 0b00c 8040000 d26765783ed9 90a42640 01
    // edde303900300c00000200234b050906001ab168271939269c02001dcf0102140025a1323031323130323435353738202020202020202098a7bd5afeb70200000100

    // 55aa 30 0b000 6050000 aefdd5020c77 1950f3a4 01
    // edde303900c30b00000200234b050906001ab168271939269c02001dcf0102140025a1323031323130323435353738202020202020202042db89d3c0650200000100




    // (20 bytes) 55 aa 30 0b 00 3a 04 00 00 74 13 46 ad 18 a3 f8 6d 2b 75 00

    // 0x55 0xaa 0x30 0x0b 0x00 0x3a 0x04 0x00 0x00 0x74 0x13 0x46 0xad 0x18 0xa3 0xf8 0x6d 0x2b 0x75 0x00
    // Random key are 6 bytes

    QByteArray data;
    QDataStream stream(&data, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::LittleEndian);
    // (20 bytes) 55 aa 30 0b 00
    stream << static_cast<quint8>(0x55); // 0: Start byte ?
    stream << static_cast<quint8>(0xaa); // 1: Static byte ?
    stream << static_cast<quint8>(MessageTypePic); // 2: Message type
    stream << static_cast<quint16>(0x0b); // 3,4: Message size
    // // 5, 6, 7, 8: Data checksum, fill in later | 3a 04 00 00
    stream << static_cast<quint32>(0);


    // 9, 10, 11, 12, 13, 14: Random bytes? | 74 13 46 ad 18 a3
    QByteArray randomBytes = QByteArray::fromHex("741346ad18a3");//generateRandomBytes();
    for (int i = 0; i < randomBytes.count(); i++) {
        stream << static_cast<quint8>(randomBytes.at(i));
    }

    // 15, 16, 17, 18: userkey uint32 | f8 6d 2b 75
    // hash quint32 from the username DEMO_MODE, maybe lts try nymea hash later
    stream << m_userKey;

    // 19: userId | 00
    stream << m_userId;

    // Calculate checksum 5, 6, 7, 8
    quint32 checksum = calculateChecksum(data.mid(9, data.length() - 9));
    data[5] = checksum & 0xff;
    data[6] = (checksum >> 8) & 0xff;
    data[7] = (checksum >> 16) & 0xff;
    data[8] = (checksum >> 24) & 0xff;

    qCDebug(dcKaco()) << "Sending PIC data request...";
    sendData(data);
}

void KacoClient::sendInverterRequest()
{
    // Testing
    sendData(QByteArray::fromHex("55aa34040063010000aa3a057a"));
    return;

    // 55 aa | 34 1000 | 84 09 00 00 | 4878 6778 8678 bcca dbca 5396 94d8 6afd
    QByteArray data;
    QDataStream stream(&data, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::LittleEndian);


    // 0, 1: Static start byte ?
    stream << static_cast<quint8>(0x55);
    stream << static_cast<quint8>(0xaa);

    // 2: Message type
    stream << static_cast<quint8>(MessageTypeIds);

    // 3, 4: Message length, fill in later
    stream << static_cast<quint16>(0x00);

    // 5, 6, 7, 8: Data checksum, fill in later
    stream << static_cast<quint32>(0);

    // The list of id hashes we are interested and parse exactly as we request them
    stream << static_cast<quint16>(0x7848); // Grid voltage L1, "g_sync.u_l_rms[0]"
    stream << static_cast<quint16>(0x7867); // Grid voltage L2, "g_sync.u_l_rms[1]"
    stream << static_cast<quint16>(0x7886); // Grid voltage L3, "g_sync.u_l_rms[2]"

    // Fill in message length 3, 4
    quint16 messageLength = data.count();
    data[3] = messageLength & 0xff;
    data[4] = (messageLength >> 8) & 0xff;

    // Fill in message checksum 5, 6, 7, 8
    quint32 checksum = calculateChecksum(data.mid(9, data.length() - 9));
    data[5] = checksum & 0xff;
    data[6] = (checksum >> 8) & 0xff;
    data[7] = (checksum >> 16) & 0xff;
    data[8] = (checksum >> 24) & 0xff;

    qCDebug(dcKaco()) << "Sending inverter data request...";
    sendData(data);
}

float KacoClient::gridVoltagePhaseA() const
{
    return m_gridVoltagePhaseA;
}

float KacoClient::gridVoltagePhaseB() const
{
    return m_gridVoltagePhaseB;
}

float KacoClient::gridVoltagePhaseC() const
{
    return m_gridVoltagePhaseC;
}

void KacoClient::connectToDevice()
{
    m_socket->connectToHost(m_hostAddress.toString(), m_port);
}

void KacoClient::disconnectFromDevice()
{
    m_socket->close();
}

void KacoClient::processResponse(const QByteArray &response)
{    
    // Data arrived while we where waiting for a message, reset
    m_requestPending = false;
    m_requestPendingTicks = 0;

    // Parse response message type
    QDataStream stream(response);
    stream.setByteOrder(QDataStream::LittleEndian);

    // Static start bytes? 0xed 0xde
    quint16 responseStart;
    stream >> responseStart;

    quint8 messageType;
    stream >> messageType;

    switch (messageType) {
    case MessageTypePic:
        processPicResponse(response);
        break;
    case MessageTypeIdsResponse:
        switch (m_state) {
        case StateRequestInverter:
            processInverterResponse(response);
            break;
        default:
            qCWarning(dcKaco()) << "Received ids response but we are currently in" << m_state << "and not expecting any data.";
            break;
        }
        break;
    default:
        qCDebug(dcKaco()) << "Unhandled message type" << byteToHexString(messageType) << messageType;
        break;
    }
}

void KacoClient::processPicResponse(const QByteArray &message)
{
    qCDebug(dcKaco()) << "Process PIC data...";
    if (message.count() >= 15) {
        quint8 picHighVersion = message.at(13);
        quint8 picLowVersion = message.at(14);
        if (picHighVersion < 2 && message.count() < 62) {
            // User type 1 ?
            qCDebug(dcKaco()) << "- User type: 1";
        }
        qCDebug(dcKaco()) << "- PIC high version:" << byteToHexString(picHighVersion) << picHighVersion;
        qCDebug(dcKaco()) << "- PIC low version:" << byteToHexString(picLowVersion) << picLowVersion;
    }

    if (message.count() >= 25) {
        QByteArray mac = message.mid(19, 6);
        qCDebug(dcKaco()) << "- MAC:" << byteArrayToHexString(mac) << mac.toHex().trimmed();
    }

    if (message.count() >= 55) {
        QByteArray serialNumber = message.mid(35, 20);
        // 68271939269c SN: 201210245578
        qCDebug(dcKaco()) << "- Serial number:" << byteArrayToHexString(serialNumber) << QString::fromUtf8(serialNumber).trimmed();
    }

    if (message.count() >= 61) {
        QByteArray randomKey = message.mid(55, 6);
        qCDebug(dcKaco()) << "- Random key:" << byteArrayToHexString(randomKey);
    }

    if (message.count() >= 62) {
        quint8 userId = message.at(61);
        qCDebug(dcKaco()) << "- User ID:" << byteToHexString(userId) << userId;
    }

    if (message.count() >= 66) {
        QByteArray clientId = message.mid(62, 4);
        QDataStream clientIdStream(&clientId, QIODevice::ReadOnly);
        clientIdStream.setByteOrder(QDataStream::LittleEndian);
        quint32 clientIdInt;
        clientIdStream >> clientIdInt;
        qCDebug(dcKaco()) << "- Client ID:" << byteArrayToHexString(clientId) << clientIdInt;
    }


    m_state = StateRequestInverter;
    qCDebug(dcKaco()) << m_state;
}

void KacoClient::processInverterResponse(const QByteArray &message)
{
    qCDebug(dcKaco()) << "Process inverter data...";

    QDataStream stream(message);
    stream.setByteOrder(QDataStream::LittleEndian);

    quint16 responseStart;
    stream >> responseStart;

    quint8 messageType;
    stream >> messageType;

    quint16 messageSize;
    stream >> messageSize;

    quint32 checksum;
    stream >> checksum;

    qCDebug(dcKaco()) << "Message start:" << byteArrayToHexString(message.left(2)) << responseStart;
    qCDebug(dcKaco()) << "Message type:" << byteToHexString(messageType) << static_cast<MessageType>(messageType);
    qCDebug(dcKaco()) << "Message size:" << byteArrayToHexString(message.mid(3,2)) << messageSize;
    qCDebug(dcKaco()) << "Message checksum:" << calculateChecksum(message.mid(9, message.length() - 9)) << "=" << checksum;

    // Parse params
    // 0400 4878 c7746a43 0400 6778 f8866c43 0400 8678 d3d16a43 0400 bcca 05292844 0400 dbca 87e60d44 0400 5396 bbfc0245 0400 94d8 9a995343 0c00 6afd 80786843801b6b4348ea6a43
    quint16 paramSize = 0;
    quint16 paramHash = 0;
    quint32 paramValueRaw = 0;

    // Grid voltage L1
    stream >> paramSize >> paramHash >> paramValueRaw;
    float gridVoltagePhaseA = convertRawValueToFloat(paramValueRaw);
    if (!qFuzzyCompare(m_gridVoltagePhaseA, gridVoltagePhaseA)) {
        qCDebug(dcKaco()) << "Grid voltage phase A" << gridVoltagePhaseA << "V";
        m_gridVoltagePhaseA = gridVoltagePhaseA;
        emit gridVoltagePhaseAChanged(m_gridVoltagePhaseA);
    }

    // Grid voltage L2
    stream >> paramSize >> paramHash >> paramValueRaw;
    float gridVoltagePhaseB = convertRawValueToFloat(paramValueRaw);
    if (!qFuzzyCompare(m_gridVoltagePhaseB, gridVoltagePhaseB)) {
        qCDebug(dcKaco()) << "Grid voltage phase B" << gridVoltagePhaseB << "V";
        m_gridVoltagePhaseB = gridVoltagePhaseB;
        emit gridVoltagePhaseBChanged(m_gridVoltagePhaseB);
    }

    // Grid voltage L3
    stream >> paramSize >> paramHash >> paramValueRaw;
    float gridVoltagePhaseC = convertRawValueToFloat(paramValueRaw);
    if (!qFuzzyCompare(m_gridVoltagePhaseC, gridVoltagePhaseC)) {
        qCDebug(dcKaco()) << "Grid voltage phase C" << gridVoltagePhaseC << "V";
        m_gridVoltagePhaseC = gridVoltagePhaseC;
        emit gridVoltagePhaseCChanged(m_gridVoltagePhaseC);
    }

}

//void KacoClient::processIds(const QByteArray &message)
//{

//    // Request: 55aa 34 1000 84090000 4878 6778 8678 bcca dbca 5396 94d8 6afd

//    // ed de 35 4800 fc1a0000 04004878c7746a4304006778f8866c4304008678d3d16a430400bcca052928440400dbca87e60d4404005396bbfc0245040094d89a9953430c006afd80786843801b6b4348ea6a43

//    //    Grid Voltage             238.4   238.8   237.5 V
//    //    VECTIS Voltage           237.3   237.3   237.3 V
//    //    PV Voltage               585.9   560.4 V
//    //    PV Power                 348.7 W
//    //    U battery                201.0 V


//    QByteArray data = message;
//    QDataStream stream(&data, QIODevice::ReadOnly);
//    stream.setByteOrder(QDataStream::LittleEndian);

//    quint16 responseStart;
//    stream >> responseStart;

//    quint8 messageType;
//    stream >> messageType;

//    quint16 messageSize;
//    stream >> messageSize;

//    quint32 checksum;
//    stream >> checksum;

//    qCDebug(dcKaco()) << "Message start:" << byteArrayToHexString(message.left(2)) << responseStart;
//    qCDebug(dcKaco()) << "Message type:" << byteToHexString(messageType) << static_cast<MessageType>(messageType);
//    qCDebug(dcKaco()) << "Message size:" << byteArrayToHexString(message.mid(3,2)) << messageSize;
//    qCDebug(dcKaco()) << "Message checksum:" << calculateChecksum(message.mid(9, message.length() - 9)) << "=" << checksum;

//    // Parse params
//    // 0400 4878 c7746a43 0400 6778 f8866c43 0400 8678 d3d16a43 0400 bcca 05292844 0400 dbca 87e60d44 0400 5396 bbfc0245 0400 94d8 9a995343 0c00 6afd 80786843801b6b4348ea6a43

//    for (int i = 9; i < message.length(); i = i + 8) {
//        // 0400 4878 c7746a43 // U L1 - "g_sync.u_l_rms[0]"
//        // 0400 6778 f8866c43 // U L2 - "g_sync.u_l_rms[1]"
//        // 0400 8678 d3d16a43 // U L3 - "g_sync.u_l_rms[2]"
//        // 0400 bcca 05292844 // U Sg1 - g_sync.u_sg_avg[0]
//        // g_sync.u_sg_avg[1]
//        // "g_sync.p_pv_lp"
//        // bms.u_total
//        // "rs.u_ext" 3 * AC voltag
//        int paramSize = static_cast<quint16>(message.at(i) & 0xFF) | (message.at(i + 1) & 0xFF) << 8;
//        int paramHash = static_cast<quint16>(message.at(i + 2) & 0xFF) | (message.at(i + 3) & 0xFF) << 8;
//        QByteArray paramData = message.mid(i + 4, paramSize);
//        QDataStream paramStream(&paramData, QIODevice::ReadOnly);
//        paramStream.setByteOrder(QDataStream::LittleEndian);
//        quint32 rawValue;
//        paramStream >> rawValue;
//        float value = 0;
//        memcpy(&value, &rawValue, sizeof(quint32));
//        qCDebug(dcKaco()) << "Param: size:" << paramSize << "hash:" << paramHash << rawValue << value;
//    }

//}

QString KacoClient::byteToHexString(quint8 byte)
{
    return QStringLiteral("0x%1").arg(byte, 2, 16, QLatin1Char('0'));
}

QString KacoClient::byteArrayToHexString(const QByteArray &byteArray)
{
    QString hexString;
    for (int i = 0; i < byteArray.count(); i++) {
        hexString.append(byteToHexString(static_cast<quint8>(byteArray.at(i))));
        if (i != byteArray.count() - 1) {
            hexString.append(" ");
        }
    }
    return hexString.toStdString().data();
}

QByteArray KacoClient::generateRandomBytes(int count)
{
    QByteArray randomBytes;
    for (int i = 0; i < count; i++) {
        randomBytes.append(static_cast<char>(rand() & 255));
    }
    return randomBytes;
}

quint16 KacoClient::getStringHash(const QString &name)
{
    return static_cast<quint16>(qHash(name) & 0xFFFF);
}

quint32 KacoClient::calculateChecksum(const QByteArray &data)
{
    quint32 checksum = 0;
    for (int i = 0; i < data.size(); i++) {
        checksum += ((data.at(i) & 0xFF) & 0xFFFFFFFF);
    }

    return checksum;
}

float KacoClient::convertRawValueToFloat(quint32 rawValue)
{
    float value = 0;
    memcpy(&value, &rawValue, sizeof(quint32));
    return value;
}

void KacoClient::refresh()
{
    if (m_requestPending) {
        m_requestPendingTicks++;
        if (m_requestPendingTicks >= 10) {
            m_requestPendingTicks = 0;
            m_requestPending = false;
            qCWarning(dcKaco()) << "No response received for" << m_state << ". Retry";
        } else {
            return;
        }
    }

    switch (m_state) {
    case StateInit:
        break;
    case StateRequestPic:
        sendPicRequest();
        break;
    case StateRequestInverter:
        sendInverterRequest();
        break;
    }
}
