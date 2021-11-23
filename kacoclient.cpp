#include "kacoclient.h"
#include "extern-plugininfo.h"

#include <QCryptographicHash>
#include <QDataStream>

#include "math.h"

KacoClient::KacoClient(const QHostAddress &hostAddress, quint16 port, const QString &password, QObject *parent) :
    QObject(parent),
    m_hostAddress(hostAddress),
    m_port(port),
    m_userPassword(password)
{
    qCDebug(dcKaco()) << "Status proprties:";
    m_statusProperties << "fm.error_bits[0]";
    m_statusProperties << "fm.error_bits[1]";
    m_statusProperties << "warn_bf";
    m_statusProperties << "prime_sm.ext_status";
    m_statusProperties << "rs.tb_status";
    m_statusProperties << "rs.dist_board_on";
    m_statusProperties << "bms.Status_BMS.Allbits";
    m_statusProperties << "prime_sm.inverter_mode";
    printHashCodes(m_statusProperties);

    qCDebug(dcKaco()) << "Demo data proprties:";
    m_demoDataProperties << "g_sync.u_l_rms[0]";
    m_demoDataProperties << "g_sync.u_l_rms[1]";
    m_demoDataProperties << "g_sync.u_l_rms[2]";
    m_demoDataProperties << "g_sync.u_sg_avg[0]";
    m_demoDataProperties << "g_sync.u_sg_avg[1]";
    m_demoDataProperties << "g_sync.p_pv_lp";
    m_demoDataProperties << "bms.u_total";
    m_demoDataProperties << "rs.u_ext";
    printHashCodes(m_demoDataProperties);

    qCDebug(dcKaco()) << "System info proprties:";
    m_systemInfoProperties << "sw_version";
    m_systemInfoProperties << "pic_version";
    m_systemInfoProperties << "rs.db_version";
    m_systemInfoProperties << "dev_serial_num";
    m_systemInfoProperties << "dev_config_txt";
    printHashCodes(m_systemInfoProperties);

    qCDebug(dcKaco()) << "Inverter proprties:";
    m_inverterProperties << "g_sync.u_l_rms[0]";
    m_inverterProperties << "g_sync.u_l_rms[1]";
    m_inverterProperties << "g_sync.u_l_rms[2]";
    m_inverterProperties << "g_sync.p_ac[0]";
    m_inverterProperties << "g_sync.p_ac[1]";
    m_inverterProperties << "g_sync.p_ac[2]";
    m_inverterProperties << "g_sync.q_ac";
    m_inverterProperties << "g_sync.u_sg_avg[0]";
    m_inverterProperties << "g_sync.u_sg_avg[1]";
    m_inverterProperties << "g_sync.p_pv_lp";
    m_inverterProperties << "gd[0].f_l_slow";
    m_inverterProperties << "iso.r";
    printHashCodes(m_inverterProperties);

    qCDebug(dcKaco()) << "Vectris proprties:";
    m_vectisProperties << "rs.p_int";
    m_vectisProperties << "rs.p_ext";
    m_vectisProperties << "rs.u_ext";
    m_vectisProperties << "rs.f_ext";
    m_vectisProperties << "rs.q_int";
    m_vectisProperties << "rs.q_ext";
    printHashCodes(m_vectisProperties);

    qCDebug(dcKaco()) << "Battery proprties:";
    m_batteryProperties << "g_sync.p_accu";
    m_batteryProperties << "bms.u_total";
    m_batteryProperties << "bms.SOEpercent_total";
    printHashCodes(m_batteryProperties);

    qCDebug(dcKaco()) << "Date proprties:";
    m_dateProperties << "rtc.SecMinHourDay";
    m_dateProperties << "rtc.DaWeMonthYear";
    printHashCodes(m_dateProperties);

    qCDebug(dcKaco()) << "Meter properties:";
    m_meterProperties << "dd.e_inverter_inj";
    m_meterProperties << "dd.e_inverter_cons";
    m_meterProperties << "dd.e_grid_inj";
    m_meterProperties << "dd.e_grid_cons";
    m_meterProperties << "dd.e_compensation";
    m_meterProperties << "dd.q_acc";
    printHashCodes(m_meterProperties);

    // 55aa 34 1400 e70a0000 aa3a 057a 4878 6778 8678 bcca dbca 5396 94d8 6afd
    // edde 35 5800 531d0000 0400 aa3a 160d2c34 0400 057a e5070a0104004878b10d6f4304006778a2397043040086784e7e6e430400bccaee2113440400dbca3ece0b440400539632dba943040094d89a994d430c006afd78b76d43688a6e43c0d76e43

    // 55aa 34 1000 84090000 4878 6778 8678 bcca dbca 5396 94d8 6afd
    // edde354800e11a0000040048783dad6e4304006778e9617043040086788dda6e430400bcca1a1f13440400dbca27c80b4404005396e784a943040094d89a994d430c006afd488d6d43809f6e43c0d76e43

    // Get the hash of the password
    m_userPasswordHash = calculateStringHashCode(m_userPassword);

    // Refresh timer
    m_refreshTimer.setInterval(1000);
    m_refreshTimer.setSingleShot(false);
    connect(&m_refreshTimer, &QTimer::timeout, this, &KacoClient::refresh);

    // TCP socket
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, [=](){
        qCDebug(dcKaco()) << "Connected to" << QString("%1:%2").arg(m_hostAddress.toString()).arg(m_port);
        emit connectedChanged(true);
        setState(StateAuthenticate);
        resetData();

        refresh();
        m_refreshTimer.start();
    });

    connect(m_socket, &QTcpSocket::disconnected, this, [=](){
        qCDebug(dcKaco()) << "Disconnected from" << QString("%1:%2").arg(m_hostAddress.toString()).arg(m_port) << m_socket->errorString();
        m_refreshTimer.stop();
        resetData();
        setState(StateNone);
        emit connectedChanged(false);
        QTimer::singleShot(2000, this, &KacoClient::connectToDevice);
    });

    connect(m_socket, &QTcpSocket::readyRead, this, [=](){
        QByteArray data = m_socket->readAll();
        qCDebug(dcKaco()) << "Data from" << QString("%1:%2").arg(m_hostAddress.toString()).arg(m_port);
        qCDebug(dcKaco()) << "<--" << qUtf8Printable(data.toHex()) << "(count:" << data.count() << ")";
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

float KacoClient::meterInverterEnergyReturnedPhaseA() const
{
    return m_meterInverterEnergyReturnedPhaseA;
}

float KacoClient::meterInverterEnergyReturnedPhaseB() const
{
    return m_meterInverterEnergyReturnedPhaseB;
}

float KacoClient::meterInverterEnergyReturnedPhaseC() const
{
    return m_meterInverterEnergyReturnedPhaseC;
}

float KacoClient::meterInverterEnergyConsumedPhaseA() const
{
    return m_meterInverterEnergyConsumedPhaseA;
}

float KacoClient::meterInverterEnergyConsumedPhaseB() const
{
    return m_meterInverterEnergyConsumedPhaseB;
}

float KacoClient::meterInverterEnergyConsumedPhaseC() const
{
    return m_meterInverterEnergyConsumedPhaseC;
}

float KacoClient::meterGridEnergyReturnedPhaseA() const
{
    return m_meterGridEnergyReturnedPhaseA;
}

float KacoClient::meterGridEnergyReturnedPhaseB() const
{
    return m_meterGridEnergyReturnedPhaseB;
}

float KacoClient::meterGridEnergyReturnedPhaseC() const
{
    return m_meterGridEnergyReturnedPhaseC;
}

float KacoClient::meterGridEnergyConsumedPhaseA() const
{
    return m_meterGridEnergyConsumedPhaseA;
}

float KacoClient::meterGridEnergyConsumedPhaseB() const
{
    return m_meterGridEnergyConsumedPhaseB;
}

float KacoClient::meterGridEnergyConsumedPhaseC() const
{
    return m_meterGridEnergyConsumedPhaseC;
}

float KacoClient::meterSelfConsumptionPhaseA() const
{
    return m_meterSelfConsumptionPhaseA;
}

float KacoClient::meterSelfConsumptionPhaseB() const
{
    return m_meterSelfConsumptionPhaseB;
}

float KacoClient::meterSelfConsumptionPhaseC() const
{
    return m_meterSelfConsumptionPhaseC;
}

bool KacoClient::sendData(const QByteArray &data)
{
    if (!connected())
        return false;

    // We sent something, we are waiting for something
    m_requestPending = true;

    qCDebug(dcKaco()) << "-->" << qUtf8Printable(data.toHex()) << "(count:" << data.count() << ")";
    int writtenBytes = m_socket->write(data);
    return (writtenBytes == data.count());
}

void KacoClient::sendPicRequest()
{
    // 55aa 30 0b00 ad030000 1b3d165f273c baa259c8 00
    // edde 30 3900 e00b0000 0200234b 05 09 06001ab168 271939269c02001dcf0102140025a13230313231303234353537382020202020202020 bfb3ae2fb0bf 00 00000000

    // 55aa 30 0b00 7e040000 991f3a2db9e0 0040e4a1 01
    // edde 30 3900 650c0000 0200234b 05 09 06001ab168 271939269c02001dcf0102140025a13230313231303234353537382020202020202020 dae8f0788096 02 00000100

    // 55aa 30 0b00 c8040000 d26765783ed9 90a42640 01
    // edde 30 3900 300c0000 0200234b 05 09 06001ab168 271939269c02001dcf0102140025a13230313231303234353537382020202020202020 98a7bd5afeb7 02 00000100

    // 55aa 30 0b00 06050000 aefdd5020c77 1950f3a4 01
    // edde 30 3900 c30b0000 0200234b 05 09 06001ab168 271939269c02001dcf0102140025a13230313231303234353537382020202020202020 42db89d3c065 02 00000100

    qCDebug(dcKaco()) << "Sending PIC data request...";

    QByteArray data;
    QDataStream stream(&data, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::LittleEndian);
    // (20 bytes) 55 aa 30 0b 00
    stream << static_cast<quint8>(0x55); // 0: Static byte
    stream << static_cast<quint8>(0xaa); // 1: Static byte
    stream << static_cast<quint8>(MessageTypePic); // 2: Message type
    stream << static_cast<quint16>(0x0b); // 3,4: Message size, fixed 20 bytes

    // 5, 6, 7, 8: Data checksum, fill in later | 3a 04 00 00
    stream << static_cast<quint32>(0);

    // 9, 10, 11, 12, 13, 14: Random key bytes
    // 15, 16, 17, 18: user key (will be overwritten later if required)
    QByteArray newRandomBytes = generateRandomBytes(10);
    qCDebug(dcKaco()) << "Fill in random bytes" << byteArrayToHexString(newRandomBytes);
    for (int i = 0; i < newRandomBytes.size(); i++)
        stream << static_cast<quint8>(newRandomBytes.at(i));

    // 19: userId
    stream << m_userId;

    // If we already received a random pic from the server, lets send the actual key encrypted
    if (m_userId != 0) {
        // 15, 16, 17, 18: userkey uint32
        QByteArray keyBuffer(4, '0');
        keyBuffer[0] = m_userPasswordHash & 0xff;
        keyBuffer[1] = (m_userPasswordHash >> 8) & 0xff;
        keyBuffer[2] = (m_userPasswordHash >> 16) & 0xff;
        keyBuffer[3] = (m_userPasswordHash >> 24) & 0xff;

        // Do some black magic with the key using the random bytes

        qCDebug(dcKaco()) << "key buffer start" << byteArrayToHexString(keyBuffer);
        qCDebug(dcKaco()) << "Using pic response random bytes" << byteArrayToHexString(m_picRandomKey);

        quint8 keyLength = keyBuffer.size();
        for (quint8 i = 0; i < keyLength && i < m_picRandomKey.size(); i++)
            keyBuffer[i] = keyBuffer.at(i) + m_picRandomKey.at(i);

        //qCDebug(dcKaco()) << "key buffer adding random bytes" << byteArrayToHexString(keyBuffer);
        quint8 iterations = 2;
        for (quint8 i = 0; i < iterations; i++) {
            keyBuffer[i % keyLength] = keyBuffer.at(i % keyLength) + 1;
            keyBuffer[i % keyLength] = keyBuffer.at(i % keyLength) + keyBuffer.at((i + 10) % keyLength);
            keyBuffer[(i + 3) % keyLength] = keyBuffer.at((i + 3) % keyLength) * keyBuffer.at((i + 11) % keyLength);
            keyBuffer[i % keyLength] = keyBuffer.at(i % keyLength) + keyBuffer.at((i + 7) % keyLength);
            //qCDebug(dcKaco()) << "key buffer after iteration" << i << byteArrayToHexString(keyBuffer);
        }

        qCDebug(dcKaco()) << "key buffer end  " << byteArrayToHexString(keyBuffer);
        data[15] = keyBuffer.at(0);
        data[16] = keyBuffer.at(1);
        data[17] = keyBuffer.at(2);
        data[18] = keyBuffer.at(3);
    }

    // Calculate checksum 5, 6, 7, 8
    quint32 checksum = calculateChecksum(data.mid(9, data.length() - 9));
    data[5] = checksum & 0xff;
    data[6] = (checksum >> 8) & 0xff;
    data[7] = (checksum >> 16) & 0xff;
    data[8] = (checksum >> 24) & 0xff;

    sendData(data);
}

void KacoClient::sendInverterRequest()
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::LittleEndian);

    // 0, 1: Static start bytes
    stream << static_cast<quint8>(0x55);
    stream << static_cast<quint8>(0xaa);

    // 2: Message type
    stream << static_cast<quint8>(MessageTypeIds);

    // 3, 4: Message length, fill in later
    stream << static_cast<quint16>(0x00);

    // 5, 6, 7, 8: Data checksum, fill in later
    stream << static_cast<quint32>(0);

    // The list of date property hashes
    foreach (const QString &dateProperty, m_dateProperties) {
        stream << static_cast<quint16>(calculateStringHashCode(dateProperty) & 0xffff);
    }

    // The list of meter property hashes
    foreach (const QString &meterProperty, m_meterProperties) {
        stream << static_cast<quint16>(calculateStringHashCode(meterProperty) & 0xffff);
    }

    // The list of inverter property hashes
    foreach (const QString &inverterProperty, m_inverterProperties) {
        stream << static_cast<quint16>(calculateStringHashCode(inverterProperty) & 0xffff);
    }

    // The list of battery property hashes
    foreach (const QString &batteryProperty, m_batteryProperties) {
        stream << static_cast<quint16>(calculateStringHashCode(batteryProperty) & 0xffff);
    }


    // Fill in message length 3, 4
    quint16 messageLength = data.count() - 9; // -9 start bytes
    data[3] = messageLength & 0xff;
    data[4] = (messageLength >> 8) & 0xff;

    // Fill in message checksum 5, 6, 7, 8
    quint32 checksum = calculateChecksum(data.mid(9, data.length() - 9));
    data[5] = checksum & 0xff;
    data[6] = (checksum >> 8) & 0xff;
    data[7] = (checksum >> 16) & 0xff;
    data[8] = (checksum >> 24) & 0xff;

    sendData(data);
}

float KacoClient::inverterGridVoltagePhaseA() const
{
    return m_inverterGridVoltagePhaseA;
}

float KacoClient::inverterGridVoltagePhaseB() const
{
    return m_inverterGridVoltagePhaseB;
}

float KacoClient::inverterGridVoltagePhaseC() const
{
    return m_inverterGridVoltagePhaseC;
}

float KacoClient::inverterPowerPhaseA() const
{
    return m_inverterPowerPhaseA;
}

float KacoClient::inverterPowerPhaseB() const
{
    return m_inverterPowerPhaseB;
}

float KacoClient::inverterPowerPhaseC() const
{
    return m_inverterPowerPhaseC;
}

float KacoClient::inverterReactivePowerPhaseA() const
{
    return m_inverterReactivePowerPhaseA;
}

float KacoClient::inverterReactivePowerPhaseB() const
{
    return m_inverterReactivePowerPhaseB;
}

float KacoClient::inverterReactivePowerPhaseC() const
{
    return m_inverterReactivePowerPhaseC;
}

float KacoClient::inverterPvVoltage1() const
{
    return m_inverterPvVoltage1;
}

float KacoClient::inverterPvVoltage2() const
{
    return m_inverterPvVoltage2;
}

float KacoClient::inverterPvPower() const
{
    return m_inverterPvPower;
}

float KacoClient::inverterGridFrequency() const
{
    return m_inverterGridFrequency;
}

float KacoClient::batteryPower() const
{
    return m_batteryPower;
}

float KacoClient::batteryVoltage() const
{
    return m_batteryVoltage;
}

float KacoClient::batteryPercentage() const
{
    return m_batteryPercentage;
}

void KacoClient::connectToDevice()
{
    m_socket->connectToHost(m_hostAddress.toString(), m_port);
}

void KacoClient::disconnectFromDevice()
{
    m_socket->close();
}

void KacoClient::setState(State state)
{
    if (m_state == state)
        return;

    m_state = state;
    emit stateChanged(m_state);
}

void KacoClient::resetData()
{
    m_requestPending = false;
    m_requestPendingTicks = 0;
    m_picCounter = 0;

    m_picHighVersion = 0;
    m_picLowVersion = 0;
    m_userType = 0;
    m_userId = 0;
    m_mac.clear();
    m_serialNumber.clear();
    m_picRandomKey.clear();
    m_clientId = 0;
    m_lastPicTimestamp = 0;

    // Properties
    m_inverterGridVoltagePhaseA = 0;
    m_inverterGridVoltagePhaseB = 0;
    m_inverterGridVoltagePhaseC = 0;
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
        m_picHighVersion = message.at(13);
        m_picLowVersion = message.at(14);
        if (m_picHighVersion < 2 && message.count() < 62) {
            // User type 1 ?
            m_userType = 1;
            qCDebug(dcKaco()) << "- User type:" << m_userType;
        }
        qCDebug(dcKaco()) << "- PIC high version:" << byteToHexString(m_picHighVersion) << m_picHighVersion;
        qCDebug(dcKaco()) << "- PIC low version:" << byteToHexString(m_picLowVersion) << m_picLowVersion;
    }

    if (message.count() >= 25) {
        m_mac = message.mid(19, 6);
        qCDebug(dcKaco()) << "- MAC:" << m_mac.toHex();
    }

    if (message.count() >= 55) {
        m_serialNumber = message.mid(35, 20);
        // 68271939269c SN: 201210245578
        qCDebug(dcKaco()) << "- Serial number:" << QString::fromUtf8(m_serialNumber).trimmed();
    }

    if (message.count() >= 61) {
        m_picRandomKey = message.mid(55, 6);
        qCDebug(dcKaco()) << "- Random key:" << byteArrayToHexString(m_picRandomKey);
    }

    if (message.count() >= 62) {
        quint8 userId = message.at(61);
        qCDebug(dcKaco()) << "- User ID:" << byteToHexString(userId) << userId;
    }

    if (message.count() >= 66) {
        QByteArray clientId = message.mid(62, 4);
        QDataStream clientIdStream(&clientId, QIODevice::ReadOnly);
        clientIdStream.setByteOrder(QDataStream::LittleEndian);
        clientIdStream >> m_clientId;
        qCDebug(dcKaco()) << "- Client ID:" << byteArrayToHexString(clientId) << m_clientId;
    }

    m_lastPicTimestamp = QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000;

    if (m_state == StateAuthenticate) {
        m_picCounter++;
        if (m_picCounter >= 1) {
            // Important: set the user id to 1 after the first cycle
            m_userId = 2;
        }

        if (m_picCounter > 3) {
            // We are done, we sent the pic 3 times successfully.
            setState(StateRequestInverter);
            return;
        }
    } else if (m_state == StateRefreshKey) {
        // Key has been refreshed successfully...continue with data fetching
        setState(StateRequestInverter);
        m_refreshTimer.stop();
        m_refreshTimer.start();
        return;
    }
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

    qCDebug(dcKaco()) << "Message start:" << responseStart << byteArrayToHexString(message.left(2));
    qCDebug(dcKaco()) << "Message type:" << static_cast<MessageType>(messageType) << byteToHexString(messageType);
    qCDebug(dcKaco()) << "Message size:" << messageSize << byteArrayToHexString(message.mid(3,2));
    qCDebug(dcKaco()) << "Message checksum:" << calculateChecksum(message.mid(9, message.length() - 9)) << "=" << checksum;

    // Parse params
    //  55aa 34 2800 1e130000 aa3a 057a 718d 256b 3454 c27c 2255 c5a0 4878 6778 8678 5c7c 7b7c 9a7c 4b3c bcca dbca 5396 b0e4 4991
    //  55aa 34 2100 8b0c0000                                         4878 6778 8678 5c7c 7b7c 9a7c 4b3c bcca dbca 5396 b0e4 4991
    //  55aa 34 3100 1e130000 aa3a 057a 718d 256b 3454 c27c 2255 c5a0 4878 6778 8678 5c7c 7b7c 9a7c 4b3c bcca dbca 5396 b0e4 4991
    // --- Date time
    // 0400 aa3a 17120a30
    // 0400 057a e5070a02
    // --- Meter
    // 0c00 718d aa0b0000e6060000507d2909
    // 0c00 256b 2600000022000000a36b0300
    // 0c00 3454 b30000004a000000abe19700
    // 0c00 c27c b8000000af0000004f523900
    // 0c00 2255 a6040000010400009cac3a03
    // 0c00 c5a0 09170000cb0d0000909fbe08
    // --- Inverter
    // 0400 4878 09596e43
    // 0400 6778 79546f43
    // 0400 8678 04aa6d43
    // 0400 5c7c 0992e242
    // 0400 7b7c b578e442
    // 0400 9a7c 8c86e542
    // 0c00 4b3c e2858cc216808dc2c03d90c2
    // 0400 bcca e4efa541
    // 0400 dbca 07163342
    // 0400 5396 01000000
    // 0400 b0e4 d4134842
    // 0400 4991 6c86484a
    // --- Battery
    // 0400 5e89 cc4c39c4
    // 0400 94d8 67665143
    // 0400 a7d3 00008842


    quint16 paramSize = 0;
    quint16 paramHash = 0;
    quint32 paramValueRaw = 0;

    // ------------- Date time

    // Time rtc.SecMinHourDay
    stream >> paramSize >> paramHash >> paramValueRaw;
    // TODO: parse time

    // Date rtc.DaWeMonthYear
    stream >> paramSize >> paramHash >> paramValueRaw;
    // TODO: parse date


    // ------------- Meter

    // Meter feed in inverter
    stream >> paramSize >> paramHash >> paramValueRaw;
    float meterInverterEnergyReturnedPhaseA = convertRawValueToFloat(paramValueRaw) / 1000.0;
    qCDebug(dcKaco()) << "Meter inverter feed in phase A" << meterInverterEnergyReturnedPhaseA << "kWh";
    if (!qFuzzyCompare(m_meterInverterEnergyReturnedPhaseA, meterInverterEnergyReturnedPhaseA)) {
        m_meterInverterEnergyReturnedPhaseA = meterInverterEnergyReturnedPhaseA;
        emit meterInverterEnergyReturnedPhaseAChanged(m_meterInverterEnergyReturnedPhaseA);
    }

    stream >> paramValueRaw;
    float meterInverterEnergyReturnedPhaseB = convertRawValueToFloat(paramValueRaw) / 1000.0;
    qCDebug(dcKaco()) << "Meter inverter feed in phase B" << meterInverterEnergyReturnedPhaseB << "kWh";
    if (!qFuzzyCompare(m_meterInverterEnergyReturnedPhaseB, meterInverterEnergyReturnedPhaseB)) {
        m_meterInverterEnergyReturnedPhaseB = meterInverterEnergyReturnedPhaseB;
        emit meterInverterEnergyReturnedPhaseBChanged(m_meterInverterEnergyReturnedPhaseB);
    }

    stream >> paramValueRaw;
    float meterInverterEnergyReturnedPhaseC = convertRawValueToFloat(paramValueRaw) / 1000.0;
    qCDebug(dcKaco()) << "Meter inverter feed in phase C" << meterInverterEnergyReturnedPhaseC << "kWh";
    if (!qFuzzyCompare(m_meterInverterEnergyReturnedPhaseC, meterInverterEnergyReturnedPhaseC)) {
        m_meterInverterEnergyReturnedPhaseC = meterInverterEnergyReturnedPhaseC;
        emit meterInverterEnergyReturnedPhaseCChanged(m_meterInverterEnergyReturnedPhaseC);
    }

    // Meter consumed inverter
    stream >> paramSize >> paramHash >> paramValueRaw;
    float meterInverterEnergyConsumedPhaseA = convertRawValueToFloat(paramValueRaw) / 1000.0;
    qCDebug(dcKaco()) << "Meter inverter consumed phase A" << meterInverterEnergyConsumedPhaseA << "kWh";
    if (!qFuzzyCompare(m_meterInverterEnergyConsumedPhaseA, meterInverterEnergyConsumedPhaseA)) {
        m_meterInverterEnergyConsumedPhaseA = meterInverterEnergyConsumedPhaseA;
        emit meterInverterEnergyConsumedPhaseAChanged(m_meterInverterEnergyConsumedPhaseA);
    }

    stream >> paramValueRaw;
    float meterInverterEnergyConsumedPhaseB = convertRawValueToFloat(paramValueRaw) / 1000.0;
    qCDebug(dcKaco()) << "Meter inverter consumed phase B" << meterInverterEnergyConsumedPhaseB << "kWh";
    if (!qFuzzyCompare(m_meterInverterEnergyConsumedPhaseB, meterInverterEnergyConsumedPhaseB)) {
        m_meterInverterEnergyConsumedPhaseB = meterInverterEnergyConsumedPhaseB;
        emit meterInverterEnergyConsumedPhaseBChanged(m_meterInverterEnergyConsumedPhaseB);
    }

    stream >> paramValueRaw;
    float meterInverterEnergyConsumedPhaseC = convertRawValueToFloat(paramValueRaw) / 1000.0;
    qCDebug(dcKaco()) << "Meter inverter consumed phase C" << meterInverterEnergyConsumedPhaseC << "kWh";
    if (!qFuzzyCompare(m_meterInverterEnergyConsumedPhaseC, meterInverterEnergyConsumedPhaseC)) {
        m_meterInverterEnergyConsumedPhaseC = meterInverterEnergyConsumedPhaseC;
        emit meterInverterEnergyConsumedPhaseCChanged(m_meterInverterEnergyConsumedPhaseC);
    }

    // Meter grid feed in
    stream >> paramSize >> paramHash >> paramValueRaw;
    float meterGridEnergyReturnedPhaseA = convertRawValueToFloat(paramValueRaw) / 1000.0;
    qCDebug(dcKaco()) << "Meter grid feed in phase A" << meterGridEnergyReturnedPhaseA << "kWh";
    if (!qFuzzyCompare(m_meterGridEnergyReturnedPhaseA, meterGridEnergyReturnedPhaseA)) {
        m_meterGridEnergyReturnedPhaseA = meterGridEnergyReturnedPhaseA;
        emit meterGridEnergyReturnedPhaseAChanged(m_meterGridEnergyReturnedPhaseA);
    }

    stream >> paramValueRaw;
    float meterGridEnergyReturnedPhaseB = convertRawValueToFloat(paramValueRaw) / 1000.0;
    qCDebug(dcKaco()) << "Meter grid feed in phase B" << meterGridEnergyReturnedPhaseB << "kWh";
    if (!qFuzzyCompare(m_meterGridEnergyReturnedPhaseB, meterGridEnergyReturnedPhaseB)) {
        m_meterGridEnergyReturnedPhaseB = meterGridEnergyReturnedPhaseB;
        emit meterGridEnergyReturnedPhaseBChanged(m_meterGridEnergyReturnedPhaseB);
    }

    stream >> paramValueRaw;
    float meterGridEnergyReturnedPhaseC = convertRawValueToFloat(paramValueRaw) / 1000.0;
    qCDebug(dcKaco()) << "Meter grid feed in phase C" << meterGridEnergyReturnedPhaseC << "kWh";
    if (!qFuzzyCompare(m_meterGridEnergyReturnedPhaseC, meterGridEnergyReturnedPhaseC)) {
        m_meterGridEnergyReturnedPhaseC = meterGridEnergyReturnedPhaseC;
        emit meterGridEnergyReturnedPhaseCChanged(m_meterGridEnergyReturnedPhaseC);
    }

    // Meter consumed grid
    stream >> paramSize >> paramHash >> paramValueRaw;
    float meterGridEnergyConsumedPhaseA = convertRawValueToFloat(paramValueRaw) / 1000.0;
    qCDebug(dcKaco()) << "Meter grid consumed phase A" << meterGridEnergyConsumedPhaseA << "kWh";
    if (!qFuzzyCompare(m_meterGridEnergyConsumedPhaseA, meterGridEnergyConsumedPhaseA)) {
        m_meterGridEnergyConsumedPhaseA = meterGridEnergyConsumedPhaseA;
        emit meterGridEnergyConsumedPhaseAChanged(m_meterGridEnergyConsumedPhaseA);
    }

    stream >> paramValueRaw;
    float meterGridEnergyConsumedPhaseB = convertRawValueToFloat(paramValueRaw) / 1000.0;
    qCDebug(dcKaco()) << "Meter grid consumed phase B" << meterGridEnergyConsumedPhaseB << "kWh";
    if (!qFuzzyCompare(m_meterGridEnergyConsumedPhaseB, meterGridEnergyConsumedPhaseB)) {
        m_meterGridEnergyConsumedPhaseB = meterGridEnergyConsumedPhaseB;
        emit meterGridEnergyConsumedPhaseBChanged(m_meterGridEnergyConsumedPhaseB);
    }

    stream >> paramValueRaw;
    float meterGridEnergyConsumedPhaseC = convertRawValueToFloat(paramValueRaw) / 1000.0;
    qCDebug(dcKaco()) << "Meter grid consumed phase C" << meterGridEnergyConsumedPhaseC << "kWh";
    if (!qFuzzyCompare(m_meterGridEnergyConsumedPhaseC, meterGridEnergyConsumedPhaseC)) {
        m_meterGridEnergyConsumedPhaseC = meterGridEnergyConsumedPhaseC;
        emit meterGridEnergyConsumedPhaseCChanged(m_meterGridEnergyConsumedPhaseC);
    }

    // Meter self consumption
    stream >> paramSize >> paramHash >> paramValueRaw;
    float meterSelfConsumptionPhaseA = convertRawValueToFloat(paramValueRaw) / 1000.0;
    qCDebug(dcKaco()) << "Meter grid consumed phase A" << meterSelfConsumptionPhaseA << "kWh";
    if (!qFuzzyCompare(m_meterSelfConsumptionPhaseA, meterSelfConsumptionPhaseA)) {
        m_meterSelfConsumptionPhaseA = meterSelfConsumptionPhaseA;
        emit meterSelfConsumptionPhaseAChanged(m_meterSelfConsumptionPhaseA);
    }

    stream >> paramValueRaw;
    float meterSelfConsumptionPhaseB = convertRawValueToFloat(paramValueRaw) / 1000.0;
    qCDebug(dcKaco()) << "Meter grid consumed phase B" << meterSelfConsumptionPhaseB << "kWh";
    if (!qFuzzyCompare(m_meterSelfConsumptionPhaseB, meterSelfConsumptionPhaseB)) {
        m_meterSelfConsumptionPhaseB = meterSelfConsumptionPhaseB;
        emit meterSelfConsumptionPhaseBChanged(m_meterSelfConsumptionPhaseB);
    }

    stream >> paramValueRaw;
    float meterSelfConsumptionPhaseC = convertRawValueToFloat(paramValueRaw) / 1000.0;
    qCDebug(dcKaco()) << "Meter grid consumed phase C" << meterSelfConsumptionPhaseC << "kWh";
    if (!qFuzzyCompare(m_meterSelfConsumptionPhaseC, meterSelfConsumptionPhaseC)) {
        m_meterSelfConsumptionPhaseC = meterSelfConsumptionPhaseC;
        emit meterSelfConsumptionPhaseCChanged(m_meterSelfConsumptionPhaseC);
    }

    // Battery AH
    stream >> paramSize >> paramHash;
    for (int i = 0; i < 3; i++)
        stream >> paramValueRaw;


    // ------------- Inverter

    // Grid voltage L1
    stream >> paramSize >> paramHash >> paramValueRaw;
    float inverterGridVoltagePhaseA = convertRawValueToFloat(paramValueRaw);
    qCDebug(dcKaco()) << "Inverter grid voltage phase A" << inverterGridVoltagePhaseA << "V";
    if (!qFuzzyCompare(m_inverterGridVoltagePhaseA, inverterGridVoltagePhaseA)) {
        m_inverterGridVoltagePhaseA = inverterGridVoltagePhaseA;
        emit inverterGridVoltagePhaseAChanged(m_inverterGridVoltagePhaseA);
    }

    // Grid voltage L2
    stream >> paramSize >> paramHash >> paramValueRaw;
    float inverterGridVoltagePhaseB = convertRawValueToFloat(paramValueRaw);
    qCDebug(dcKaco()) << "Inverter grid voltage phase B" << inverterGridVoltagePhaseB << "V";
    if (!qFuzzyCompare(m_inverterGridVoltagePhaseB, inverterGridVoltagePhaseB)) {
        m_inverterGridVoltagePhaseB = inverterGridVoltagePhaseB;
        emit inverterGridVoltagePhaseBChanged(m_inverterGridVoltagePhaseB);
    }

    // Grid voltage L3
    stream >> paramSize >> paramHash >> paramValueRaw;
    float inverterGridVoltagePhaseC = convertRawValueToFloat(paramValueRaw);
    qCDebug(dcKaco()) << "Inverter grid voltage phase C" << inverterGridVoltagePhaseC << "V";
    if (!qFuzzyCompare(m_inverterGridVoltagePhaseC, inverterGridVoltagePhaseC)) {
        m_inverterGridVoltagePhaseC = inverterGridVoltagePhaseC;
        emit inverterGridVoltagePhaseCChanged(m_inverterGridVoltagePhaseC);
    }

    // AC Power L1
    stream >> paramSize >> paramHash >> paramValueRaw;
    float inverterPowerPhaseA = convertRawValueToFloat(paramValueRaw);
    qCDebug(dcKaco()) << "Inverter power phase A" << inverterPowerPhaseA << "W";
    if (!qFuzzyCompare(m_inverterPowerPhaseA, inverterPowerPhaseA)) {
        m_inverterPowerPhaseA = inverterPowerPhaseA;
        emit inverterPowerPhaseAChanged(m_inverterPowerPhaseA);
    }

    // AC Power L2
    stream >> paramSize >> paramHash >> paramValueRaw;
    float inverterPowerPhaseB = convertRawValueToFloat(paramValueRaw);
    qCDebug(dcKaco()) << "Inverter power phase B" << inverterPowerPhaseB << "W";
    if (!qFuzzyCompare(m_inverterPowerPhaseB, inverterPowerPhaseB)) {
        m_inverterPowerPhaseB = inverterPowerPhaseB;
        emit inverterPowerPhaseBChanged(m_inverterPowerPhaseB);
    }

    // AC Power L3
    stream >> paramSize >> paramHash >> paramValueRaw;
    float inverterPowerPhaseC = convertRawValueToFloat(paramValueRaw);
    qCDebug(dcKaco()) << "Inverter power phase C" << inverterPowerPhaseC << "W";
    if (!qFuzzyCompare(m_inverterPowerPhaseC, inverterPowerPhaseC)) {
        m_inverterPowerPhaseC = inverterPowerPhaseC;
        emit inverterPowerPhaseCChanged(m_inverterPowerPhaseC);
    }

    // AC reactive Power L1
    stream >> paramSize >> paramHash >> paramValueRaw;
    float inverterReactivePowerPhaseA = convertRawValueToFloat(paramValueRaw);
    qCDebug(dcKaco()) << "Inverter reactive power phase A" << inverterReactivePowerPhaseA << "Var";
    if (!qFuzzyCompare(m_inverterReactivePowerPhaseA, inverterReactivePowerPhaseA)) {
        m_inverterReactivePowerPhaseA = inverterReactivePowerPhaseA;
        emit inverterReactivePowerPhaseAChanged(m_inverterReactivePowerPhaseA);
    }

    // AC reactive Power L2
    stream >> paramValueRaw;
    float inverterReactivePowerPhaseB = convertRawValueToFloat(paramValueRaw);
    qCDebug(dcKaco()) << "Inverter reactive power phase B" << inverterReactivePowerPhaseB << "Var";
    if (!qFuzzyCompare(m_inverterReactivePowerPhaseB, inverterReactivePowerPhaseB)) {
        m_inverterReactivePowerPhaseB = inverterReactivePowerPhaseB;
        emit inverterReactivePowerPhaseBChanged(m_inverterReactivePowerPhaseB);
    }

    // AC reactive Power L2
    stream >> paramValueRaw;
    float inverterReactivePowerPhaseC = convertRawValueToFloat(paramValueRaw);
    qCDebug(dcKaco()) << "Inverter reactive power phase C" << inverterReactivePowerPhaseC << "Var";
    if (!qFuzzyCompare(m_inverterReactivePowerPhaseC, inverterReactivePowerPhaseC)) {
        m_inverterReactivePowerPhaseC = inverterReactivePowerPhaseC;
        emit inverterReactivePowerPhaseCChanged(m_inverterReactivePowerPhaseC);
    }

    // PV voltage 1
    stream >> paramSize >> paramHash >> paramValueRaw;
    float inverterPvVoltage1 = convertRawValueToFloat(paramValueRaw);
    qCDebug(dcKaco()) << "Inverter PV Voltage 1" << inverterPvVoltage1 << "V";
    if (!qFuzzyCompare(m_inverterPvVoltage1, inverterPvVoltage1)) {
        m_inverterPvVoltage1 = inverterPvVoltage1;
        emit inverterPvVoltage1Changed(m_inverterPvVoltage1);
    }

    // PV voltage 2
    stream >> paramSize >> paramHash >> paramValueRaw;
    float inverterPvVoltage2 = convertRawValueToFloat(paramValueRaw);
    qCDebug(dcKaco()) << "Inverter PV Voltage 2" << inverterPvVoltage2 << "V";
    if (!qFuzzyCompare(m_inverterPvVoltage2, inverterPvVoltage2)) {
        m_inverterPvVoltage2 = inverterPvVoltage2;
        emit inverterPvVoltage2Changed(m_inverterPvVoltage2);
    }

    // PV power
    stream >> paramSize >> paramHash >> paramValueRaw;
    float inverterPvPower = convertRawValueToFloat(paramValueRaw);
    qCDebug(dcKaco()) << "Inverter PV power" << inverterPvPower << "W";
    if (!qFuzzyCompare(m_inverterPvPower, inverterPvPower)) {
        m_inverterPvPower = inverterPvPower;
        emit inverterPvPowerChanged(m_inverterPvPower);
    }

    // Grid frequency
    stream >> paramSize >> paramHash >> paramValueRaw;
    float inverterGridFrequency = convertRawValueToFloat(paramValueRaw);
    qCDebug(dcKaco()) << "Inverter grid frequency" << inverterGridFrequency << "Hz";
    if (!qFuzzyCompare(m_inverterGridFrequency, inverterGridFrequency)) {
        m_inverterGridFrequency = inverterGridFrequency;
        emit inverterGridFrequencyChanged(m_inverterGridFrequency);
    }

    // R isolation
    stream >> paramSize >> paramHash >> paramValueRaw;
    float inverterResistanceIsolation = convertRawValueToFloat(paramValueRaw);
    qCDebug(dcKaco()) << "Inverter isolation resistence" << inverterResistanceIsolation << "Ohm";
    if (!qFuzzyCompare(m_inverterResistanceIsolation, inverterResistanceIsolation)) {
        m_inverterResistanceIsolation = inverterResistanceIsolation;
        emit inverterResistanceIsolationChanged(m_inverterResistanceIsolation);
    }

    // ------------- Battery
    stream >> paramSize >> paramHash >> paramValueRaw;
    float batteryPower = convertRawValueToFloat(paramValueRaw);
    qCDebug(dcKaco()) << "Battery power" << batteryPower << "W";
    if (!qFuzzyCompare(m_batteryPower, batteryPower)) {
        m_batteryPower = batteryPower;
        emit batteryPowerChanged(m_batteryPower);
    }

    stream >> paramSize >> paramHash >> paramValueRaw;
    float batteryVoltage = convertRawValueToFloat(paramValueRaw);
    qCDebug(dcKaco()) << "Battery voltage" << batteryVoltage << "V";
    if (!qFuzzyCompare(m_batteryVoltage, batteryVoltage)) {
        m_batteryVoltage = batteryVoltage;
        emit batteryVoltageChanged(m_batteryVoltage);
    }

    stream >> paramSize >> paramHash >> paramValueRaw;
    float batteryPercentage = convertRawValueToFloat(paramValueRaw);
    qCDebug(dcKaco()) << "Battery SOE" << batteryPercentage << "%";
    if (!qFuzzyCompare(m_batteryPercentage, batteryPercentage)) {
        m_batteryPercentage = batteryPercentage;
        emit batteryPercentageChanged(m_batteryPercentage);
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

qint32 KacoClient::calculateStringHashCode(const QString &name)
{
    qint32 hashSum = 0;
    for (int i = 0; i < name.length(); i++)
        hashSum = (hashSum * 31) + name.at(i).toLatin1();

    //qCDebug(dcKaco()) << "Hash of" << name << hashSum;
    return hashSum;
}

quint32 KacoClient::calculateChecksum(const QByteArray &data)
{
    quint32 checksum = 0;
    for (int i = 0; i < data.size(); i++)
        checksum += ((data.at(i) & 0xFF) & 0xFFFFFFFF);

    return checksum;
}

float KacoClient::convertRawValueToFloat(quint32 rawValue)
{
    float value = 0;
    memcpy(&value, &rawValue, sizeof(quint32));
    return value;
}

QByteArray KacoClient::convertUint32ToByteArrayLittleEndian(quint32 value)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << value;
    return data;
}

bool KacoClient::picRefreshRequired()
{
    uint secondsSinceLastRefresh = (QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000.0) - m_lastPicTimestamp;
    return (secondsSinceLastRefresh) >= 5;
}

void KacoClient::printHashCodes(const QStringList &properties)
{
    foreach (const QString &status, properties) {
        QByteArray data;
        QDataStream stream(&data, QIODevice::ReadWrite);
        stream.setByteOrder(QDataStream::LittleEndian);
        quint16 hashCode = calculateStringHashCode(status) & 0xffff;
        stream << hashCode;
        qCDebug(dcKaco()) << " -" << data.toHex() << "|" << status << byteArrayToHexString(data);
    }
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

    if (m_state != StateAuthenticate && picRefreshRequired()) {
        qCDebug(dcKaco()) << "Refreshing key required...";
        setState(StateRefreshKey);
    }

    switch (m_state) {
    case StateNone:
        break;
    case StateAuthenticate:
    case StateRefreshKey:
        sendPicRequest();
        break;
    case StateRequestInverter:
        sendInverterRequest();
        break;
    }
}






