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

#include "kacoclient.h"
#include "extern-plugininfo.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>
#include <iterator>
#include <sstream>

#include "math.h"
#include "integrationpluginkacobh10.h"

KacoClient::KacoClient(const QHostAddress &hostAddress, quint16 port, const QString &password, QObject *parent) :
    QObject(parent),
    m_hostAddress(hostAddress),
    m_port(port),
    m_userPassword(password)
{
    // Initialize property hashes for requests and data parsing
    m_statusProperties << "fm.error_bits[0]";
    m_statusProperties << "fm.error_bits[1]";
    m_statusProperties << "warn_bf";
    m_statusProperties << "prime_sm.ext_status";
    m_statusProperties << "rs.tb_status";
    m_statusProperties << "rs.dist_board_on";
    m_statusProperties << "bms.Status_BMS.Allbits";
    m_statusProperties << "prime_sm.inverter_mode";
    foreach (const QString &property, m_statusProperties)
        m_propertyHashes.insert(calculateStringHashCode(property) & 0xffff, property);

    m_systemInfoProperties << "sw_version"; // controller version
    //m_systemInfoProperties << "pic_version";
    m_systemInfoProperties << "rs.db_version"; // hy-switch version
    //m_systemInfoProperties << "dev_serial_num";
    //m_systemInfoProperties << "dev_config_txt";
    foreach (const QString &property, m_systemInfoProperties)
        m_propertyHashes.insert(calculateStringHashCode(property) & 0xffff, property);

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
    foreach (const QString &property, m_inverterProperties)
        m_propertyHashes.insert(calculateStringHashCode(property) & 0xffff, property);

    m_vectisProperties << "rs.p_int";
    m_vectisProperties << "rs.p_ext";
    m_vectisProperties << "rs.u_ext";
    m_vectisProperties << "rs.f_ext";
    //m_vectisProperties << "rs.q_int";
    //m_vectisProperties << "rs.q_ext";
    foreach (const QString &property, m_vectisProperties)
        m_propertyHashes.insert(calculateStringHashCode(property) & 0xffff, property);

    m_batteryProperties << "g_sync.p_accu";
    m_batteryProperties << "bms.u_total";
    m_batteryProperties << "bms.SOEpercent_total";
    foreach (const QString &property, m_batteryProperties)
        m_propertyHashes.insert(calculateStringHashCode(property) & 0xffff, property);

    m_dateProperties << "rtc.SecMinHourDay";
    m_dateProperties << "rtc.DaWeMonthYear";
    foreach (const QString &property, m_dateProperties)
        m_propertyHashes.insert(calculateStringHashCode(property) & 0xffff, property);

    m_meterProperties << "dd.e_inverter_inj";
    m_meterProperties << "dd.e_inv_feedin";     // Test
    m_meterProperties << "dd.e_inverter_cons";
    m_meterProperties << "dd.e_inv_cons";       // Test
    m_meterProperties << "dd.e_grid_inj";
    m_meterProperties << "dd.e_grid_feedin";    // Test
    m_meterProperties << "dd.e_grid_cons";
    m_meterProperties << "dd.e_compensation";
    m_meterProperties << "dd.q_acc";
    foreach (const QString &property, m_meterProperties)
        m_propertyHashes.insert(calculateStringHashCode(property) & 0xffff, property);

    // Show for debugging
    //    foreach (const QString &property, m_propertyHashes.values())
    //        qCDebug(dcKacoBh10()) << "-->" << property << m_propertyHashes.key(property);

    // Get the hash of the password
    m_userPasswordHash = calculateStringHashCode(m_userPassword);

    // Refresh timer
    m_refreshTimer.setInterval(1000);
    m_refreshTimer.setSingleShot(false);
    connect(&m_refreshTimer, &QTimer::timeout, this, &KacoClient::refresh);

    // Reconnect timer. Tries every 30s to find the device again.
    m_reconnectTimer.setInterval(30000);
    m_reconnectTimer.setSingleShot(false);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &KacoClient::connectToDevice);

    // TCP socket
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, [=](){
        qCDebug(dcKacoBh10()) << "Connected to" << QString("%1:%2").arg(m_hostAddress.toString()).arg(m_port);
        //emit connectedChanged(true); Emit connected only when the device is actually answering.
        setState(StateAuthenticate);
        resetData();

        //refresh();
        m_refreshTimer.start();
    });

    connect(m_socket, &QTcpSocket::disconnected, this, [=](){
        qCDebug(dcKacoBh10()) << "Disconnected from" << QString("%1:%2").arg(m_hostAddress.toString()).arg(m_port) << m_socket->errorString();
        m_refreshTimer.stop();
        resetData();
        setState(StateNone);
        emit connectedChanged(false);
        QTimer::singleShot(2000, this, &KacoClient::connectToDevice);
        m_reconnectTimer.start();
    });

    connect(m_socket, &QTcpSocket::readyRead, this, [=](){
        QByteArray data = m_socket->readAll();
        qCDebug(dcKacoBh10()) << "Data from" << QString("%1:%2").arg(m_hostAddress.toString()).arg(m_port);
        qCDebug(dcKacoBh10()) << "<--" << qUtf8Printable(data.toHex()) << "(count:" << data.count() << ")";
        processResponse(data);
    });

    connect(m_socket, &QTcpSocket::stateChanged, this, [=](QAbstractSocket::SocketState socketState){
        qCDebug(dcKacoBh10()) << "Socket state changed" << QString("%1:%2").arg(m_hostAddress.toString()).arg(m_port) << socketState;
    });
}

bool KacoClient::connected() const
{
    return (m_socket->state() == QAbstractSocket::ConnectedState);
}

QString KacoClient::serialNumber() const
{
    return m_serialNumber;
}

float KacoClient::meterInverterEnergyReturnedToday() const
{
    return m_meterInverterEnergyReturnedToday;
}

float KacoClient::meterInverterEnergyReturnedMonth() const
{
    return m_meterInverterEnergyReturnedMonth;
}

float KacoClient::meterInverterEnergyReturnedTotal() const
{
    return m_meterInverterEnergyReturnedTotal;
}

float KacoClient::meterInverterEnergyConsumedToday() const
{
    return m_meterInverterEnergyConsumedToday;
}

float KacoClient::meterInverterEnergyConsumedMonth() const
{
    return m_meterInverterEnergyConsumedMonth;
}

float KacoClient::meterInverterEnergyConsumedTotal() const
{
    return m_meterInverterEnergyConsumedTotal;
}

float KacoClient::meterGridEnergyReturnedToday() const
{
    return m_meterGridEnergyReturnedToday;
}

float KacoClient::meterGridEnergyReturnedMonth() const
{
    return m_meterGridEnergyReturnedMonth;
}

float KacoClient::meterGridEnergyReturnedTotal() const
{
    return m_meterGridEnergyReturnedTotal;
}

float KacoClient::meterGridEnergyConsumedToday() const
{
    return m_meterGridEnergyConsumedToday;
}

float KacoClient::meterGridEnergyConsumedMonth() const
{
    return m_meterGridEnergyConsumedMonth;
}

float KacoClient::meterGridEnergyConsumedTotal() const
{
    return m_meterGridEnergyConsumedTotal;
}

float KacoClient::meterSelfConsumptionDay() const
{
    return m_meterSelfConsumptionDay;
}

float KacoClient::meterSelfConsumptionMonth() const
{
    return m_meterSelfConsumptionMonth;
}

float KacoClient::meterSelfConsumptionTotal() const
{
    return m_meterSelfConsumptionTotal;
}

float KacoClient::meterAhBatteryToday() const
{
    return m_meterAhBatteryToday;
}

float KacoClient::meterAhBatteryMonth() const
{
    return m_meterAhBatteryMonth;
}

float KacoClient::meterAhBatteryTotal() const
{
    return m_meterAhBatteryTotal;
}

float KacoClient::meterVoltagePhaseA() const
{
    return m_meterVoltagePhaseA;
}

float KacoClient::meterVoltagePhaseB() const
{
    return m_meterVoltagePhaseB;
}

float KacoClient::meterVoltagePhaseC() const
{
    return m_meterVoltagePhaseC;
}

float KacoClient::meterPowerPhaseA() const
{
    return m_meterPowerPhaseA;
}

float KacoClient::meterPowerPhaseB() const
{
    return m_meterPowerPhaseB;
}

float KacoClient::meterPowerPhaseC() const
{
    return m_meterPowerPhaseC;
}

float KacoClient::meterPowerInternalPhaseA() const
{
    return m_meterPowerInternalPhaseA;
}

float KacoClient::meterPowerInternalPhaseB() const
{
    return m_meterPowerInternalPhaseB;
}

float KacoClient::meterPowerInternalPhaseC() const
{
    return m_meterPowerInternalPhaseC;
}

float KacoClient::meterFrequency() const
{
    return m_meterFrequency;
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

float KacoClient::inverterFrequency() const
{
    return m_inverterFrequency;
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
    m_reconnectTimer.start();
    m_socket->connectToHost(m_hostAddress.toString(), m_port);
}

void KacoClient::disconnectFromDevice()
{
    m_socket->close();
    m_reconnectTimer.stop();
}

void KacoClient::setHostAddress(QHostAddress hostAddress)
{
    m_hostAddress = hostAddress;
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
    m_comVersion.clear();
    m_controllerVersion.clear();
    m_hyswitchVersion.clear();
    m_picRandomKey.clear();
    m_clientId = 0;
    m_lastPicTimestamp = 0;
    m_authorization = false;
    emit authorizationChanged(false);

    // Properties

    // Meter information
    m_meterInverterEnergyReturnedToday = 0;
    m_meterInverterEnergyReturnedMonth = 0;
    m_meterInverterEnergyReturnedTotal = 0;
    m_meterInverterEnergyConsumedToday = 0;
    m_meterInverterEnergyConsumedMonth = 0;
    m_meterInverterEnergyConsumedTotal = 0;
    m_meterGridEnergyReturnedToday = 0;
    m_meterGridEnergyReturnedMonth = 0;
    m_meterGridEnergyReturnedTotal = 0;
    m_meterGridEnergyConsumedToday = 0;
    m_meterGridEnergyConsumedMonth = 0;
    m_meterGridEnergyConsumedTotal = 0;
    m_meterSelfConsumptionDay = 0;
    m_meterSelfConsumptionMonth = 0;
    m_meterSelfConsumptionTotal = 0;
    m_meterAhBatteryToday = 0;
    m_meterAhBatteryMonth = 0;
    m_meterAhBatteryTotal = 0;

    m_meterVoltagePhaseA = 0;
    m_meterVoltagePhaseB = 0;
    m_meterVoltagePhaseC = 0;
    m_meterPowerPhaseA = 0;
    m_meterPowerPhaseB = 0;
    m_meterPowerPhaseC = 0;
    m_meterPowerInternalPhaseA = 0;
    m_meterPowerInternalPhaseB = 0;
    m_meterPowerInternalPhaseC = 0;
    m_meterFrequency = 0;

    // Inverter information
    m_inverterGridVoltagePhaseA = 0;
    m_inverterGridVoltagePhaseB = 0;
    m_inverterGridVoltagePhaseC = 0;
    m_inverterPowerPhaseA = 0;
    m_inverterPowerPhaseB = 0;
    m_inverterPowerPhaseC = 0;
    m_inverterReactivePowerPhaseA = 0;
    m_inverterReactivePowerPhaseB = 0;
    m_inverterReactivePowerPhaseC = 0;
    m_inverterPvVoltage1 = 0;
    m_inverterPvVoltage2 = 0;
    m_inverterPvPower = 0;
    m_inverterFrequency = 0;
    m_inverterResistanceIsolation = 0;

    // Battery information
    m_batteryPower = 0;
    m_batteryVoltage = 0;
    m_batteryPercentage = 0;
}

bool KacoClient::sendData(const QByteArray &data)
{
    if (!connected())
        return false;

    // We sent something, we are waiting for something
    m_requestPending = true;

    qCDebug(dcKacoBh10()) << "-->" << qUtf8Printable(data.toHex()) << "(count:" << data.count() << ")";
    int writtenBytes = m_socket->write(data);
    return (writtenBytes == data.count());
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

    if (responseStart != static_cast<quint16>(0xdeed)) {    // 0xed 0xde is swapped in "responseStart".
        qCWarning(dcKacoBh10()) << "Configured device is not a Kaco BH10.";
        qCWarning(dcKacoBh10()) << "Start of message is" << byteArrayToHexString(response.left(2)) << ", expected \"0xed 0xde\".";
    }

    emit connectedChanged(true);
    m_reconnectTimer.stop();

    quint8 messageType;
    stream >> messageType;

    switch (messageType) {
    case MessageTypePic:
        processPicResponse(response);
        break;
    case MessageTypeIdsResponse:
        switch (m_state) {
        case StateRefreshData:
            processInverterResponse(response);
            break;
        default:
            qCWarning(dcKacoBh10()) << "Received ids response but we are currently in" << m_state << "and not expecting any data.";
            break;
        }
        break;
    default:
        qCDebug(dcKacoBh10()) << "Unhandled message type" << byteToHexString(messageType) << messageType;
        break;
    }
}

void KacoClient::sendPicRequest()
{
    // Login flow: Send pic 4 times, first only random byte, the following 3 using black magic with the key sent from the inverter
    // Once done successfully, we have to refresh the key every 5 seconds

    // Request:  55aa 30 0b00 ad030000 1b3d165f273c baa259c8 00
    // Response: edde 30 3900 e00b0000 0200234b 05 09 06001ab168 271939269c02001dcf0102140025a13230313231303234353537382020202020202020 bfb3ae2fb0bf 00 00000000

    // Request:  55aa 30 0b00 7e040000 991f3a2db9e0 0040e4a1 01
    // Response: edde 30 3900 650c0000 0200234b 05 09 06001ab168 271939269c02001dcf0102140025a13230313231303234353537382020202020202020 dae8f0788096 02 00000100

    // Request:  55aa 30 0b00 c8040000 d26765783ed9 90a42640 01
    // Response: edde 30 3900 300c0000 0200234b 05 09 06001ab168 271939269c02001dcf0102140025a13230313231303234353537382020202020202020 98a7bd5afeb7 02 00000100

    // Request:  55aa 30 0b00 06050000 aefdd5020c77 1950f3a4 01
    // Response: edde 30 3900 c30b0000 0200234b 05 09 06001ab168 271939269c02001dcf0102140025a13230313231303234353537382020202020202020 42db89d3c065 02 00000100

    qCDebug(dcKacoBh10()) << "Sending PIC request...";

    QByteArray payload;
    QDataStream stream(&payload, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::LittleEndian);

    // 0 - 5: Random key bytes 6 bytes
    // 6 - 9: user key 4 bytes (will be encrypted later if not the first message)
    QByteArray newRandomBytes = generateRandomBytes(10);
    //qCDebug(dcKacoBh10()) << "Fill in random bytes" << byteArrayToHexString(newRandomBytes);
    for (int i = 0; i < newRandomBytes.size(); i++)
        stream << static_cast<quint8>(newRandomBytes.at(i));

    // 10: userId
    //stream << m_userId;
    stream << static_cast<quint8>(0x00);

    // m_userId is 0 on startup and changes to 2 on the first received response. Here, it acts as a "have we received anything yet?" test.
    if (m_userId != 0) {
        if (m_communicationVer8x) {

            if (m_mac.size() > 0) {
                shuffleBytes(m_mac, 0, 6, m_picRandomKey, payload, 0, 99);
            } else {
                qCWarning(dcKacoBh10()) << "No mac received, cannot send PIC request.";
                return;
            }

            payload[6] = m_userPasswordHash & 0xff;
            payload[7] = (m_userPasswordHash >> 8) & 0xff;
            payload[8] = (m_userPasswordHash >> 16) & 0xff;
            payload[9] = (m_userPasswordHash >> 24) & 0xff;
            shuffleBytes(payload, 6, 4, m_picRandomKey, payload, 6, 2);
            QByteArray tmp = updateIdentKey(m_picRandomKey);
            for (int i = 0; i < tmp.size(); i++)
                stream << static_cast<quint8>(tmp.at(i));

        } else {
            // If we already received a random pic key from the server,
            // lets send the actual key encrypted using black magic
            payload[6] = m_userPasswordHash & 0xff;
            payload[7] = (m_userPasswordHash >> 8) & 0xff;
            payload[8] = (m_userPasswordHash >> 16) & 0xff;
            payload[9] = (m_userPasswordHash >> 24) & 0xff;
            shuffleBytes(payload, 6, 4, m_picRandomKey, payload, 6, 2);
            payload[10] = m_userId & 0xff;
        }
    }

    sendData(buildPackage(MessageTypePic, payload));
}

void KacoClient::shuffleBytes(const QByteArray &src, int spos, int len, const QByteArray &key, QByteArray &dest, int dpos, int k)
{
    QByteArray tmp;
    tmp.resize(len);
    if (src.size() < spos + len) {
        qCWarning(dcKacoBh10()) << "Source array out of bounds.";
        return;
    }
    for (int i{0}; i < len; i++)
        tmp[i] = src.at(spos + i);

    for (int i{0}; i < len && i < key.size(); i++)
        tmp[i] = tmp.at(i) + key.at(i);

    for (int i{0}; i < k; i++) {
        tmp[i % len] = tmp.at(i % len) + 1;
        tmp[i % len] = tmp.at(i % len) + tmp.at((i + 10) % len);
        tmp[(i + 3) % len] = tmp.at((i + 3) % len) * tmp.at((i + 11) % len);
        tmp[i % len] = tmp.at(i % len) + tmp.at((i + 7) % len);
    }

    if (dest.size() < dpos + len) {
        qCWarning(dcKacoBh10()) << "Destination array out of bounds.";
        return;
    }
    for (int i{0}; i < len; i++)
        dest[dpos + i] = tmp.at(i);
}

QByteArray KacoClient::updateIdentKey(const QByteArray &randomKey)
{
    // Load ident key if it is not loaded already.
    if (m_identKey.size() < 1) {

        // Read ident key file. Directory and filename are specified here.
        QDir fileFolder("/root/kaco");
        if (!fileFolder.exists()) {
            qCWarning(dcKacoBh10()) << "Ident key file should be in directory " + fileFolder.path() + "/, but that directory does not exist.";
            return randomKey;
        }

        QString fileName = "ident.key";
        QString filePath = fileFolder.path() + "/" + fileName;
        bool fileExists = QFileInfo::exists(filePath) && QFileInfo(filePath).isFile();
        if (!fileExists) {
            qCWarning(dcKacoBh10()) << "Ident key file " + fileName + " should be in directory " + fileFolder.path() + "/, but that file does not exist.";
            return randomKey;
        }

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCWarning(dcKacoBh10()) << "Could not open ident key file " + fileName + " in directory " + fileFolder.path() + "/";
            return randomKey;
        }

        QTextStream in(&file);
        QString identKeyString = in.readLine();
        file.close();
        qCDebug(dcKacoBh10()) << "Contents of ident key file " + fileName + " in directory " + fileFolder.path() + "/ read: " + identKeyString;

        // Check contents of ident key file and transform to a byte array.
        int stringLength = identKeyString.length();
        std::stringstream ss;
        if (stringLength % 2) {
            qCWarning(dcKacoBh10()) << "Ident key file " + fileName + " in directory " + fileFolder.path() + "/ seems to be invalid.";
            return randomKey;
        }

        for (const auto &item : qAsConst(identKeyString)) {
            if (item.isLetterOrNumber()) {
                ss << item.toLatin1();
            } else {
                qCWarning(dcKacoBh10()) << "Ident key file " + fileName + " in directory " + fileFolder.path() + "/ seems to be invalid.";
                return randomKey;
            }
        }
        std::string stringConversion = ss.str();
        int byteCount = stringLength / 2;
        m_identKey.resize(byteCount);
        std::string substring;
        for (int i{0}; i < byteCount; ++i) {
            substring = stringConversion.substr(i * 2, 2);
            m_identKey[i] = std::stoul(substring, nullptr, 16); // Convert a hex number string to number.
        }
    }

    quint8 len = 8;
    QByteArray tmp;
    tmp.resize(len);
    shuffleBytes(m_identKey, 0, len, randomKey, tmp, 0, 99);

    return tmp;
}

void KacoClient::processPicResponse(const QByteArray &message)
{
    qCDebug(dcKacoBh10()) << "Process PIC data...";
    if (message.count() >= 15) {
        m_picHighVersion = message.at(13);
        m_picLowVersion = message.at(14);
        if (m_picHighVersion < 2 && message.count() < 62) {
            // User type 1 ?
            m_userType = 1;
            qCDebug(dcKacoBh10()) << "- User type:" << m_userType;
        }
        if (m_picHighVersion < 8) {
            m_communicationVer8x = false;
        } else {
            m_communicationVer8x = true;
        }

        QString comVersion{QString("%1.%2").arg(m_picHighVersion).arg(m_picLowVersion)};
        qCDebug(dcKacoBh10()) << "- Inverter com version:" << comVersion;
        if (m_comVersion != comVersion) {
            m_comVersion = comVersion;
            emit comVersionChanged(m_comVersion);
        }

    }

    if (message.count() >= 25) {
        m_mac = message.mid(19, 6);
        qCDebug(dcKacoBh10()) << "- MAC:" << m_mac.toHex();
    }

    if (message.count() >= 55) {
        QString serialNumber = QString::fromUtf8(message.mid(35, 20)).trimmed();
        // 68271939269c SN: 201210245578
        qCDebug(dcKacoBh10()) << "- Serial number:" << m_serialNumber;
        if (m_serialNumber != serialNumber) {
            m_serialNumber = serialNumber;
            emit serialNumberChanged(m_serialNumber);
        }
    }

    if (message.count() >= 61) {
        m_picRandomKey = message.mid(55, 6);
        qCDebug(dcKacoBh10()) << "- Random key:" << byteArrayToHexString(m_picRandomKey);
    }

    if (message.count() >= 62) {
        quint8 userId = message.at(61);
        if (m_communicationVer8x) {
            if (userId > 7) {
                qCWarning(dcKacoBh10()) << "- Error: Multiple devices try to accessed this inverter. Response invalid.";
            } else if (userId > 0) {
                if ((userId & 0b010) == 0b010) {
                    qCDebug(dcKacoBh10()) << "- Access Status: Ident Key Accepted.";
                } else {
                    qCWarning(dcKacoBh10()) << "- Access Status: Ident Key Reject.";
                }
                if ((userId & 0b0100) == 0b0100) {
                    qCDebug(dcKacoBh10()) << "- Access Status: User Key Accepted.";
                } else {
                    // Changed from qCWarning to qCDebug. Apparently correct user key is not needed to get values
                    // (did that change with recent firmware?). The warning messages were spamming the log, eating
                    // up too much disk space.
                    qCDebug(dcKacoBh10()) << "- Access Status: User Key Reject.";
                }
            }
        } else {
            qCDebug(dcKacoBh10()) << "- User ID:" << byteToHexString(userId) << userId;
        }

    }

    if (message.count() >= 66) {
        QByteArray clientId = message.mid(62, 4);
        QDataStream clientIdStream(&clientId, QIODevice::ReadOnly);
        clientIdStream.setByteOrder(QDataStream::LittleEndian);
        clientIdStream >> m_clientId;
        qCDebug(dcKacoBh10()) << "- Client ID:" << byteArrayToHexString(clientId) << m_clientId;
    }

    // Unsigned rollover is used intentionally here.
    m_lastPicTimestamp = QDateTime::currentDateTime().toMSecsSinceEpoch();
    qCDebug(dcKacoBh10()) << "Creating Timestamp:" << m_lastPicTimestamp;

    if (m_state == StateAuthenticate) {
        m_picCounter++;
        qCDebug(dcKacoBh10()) << "State is authenticate, pic counter is:" << m_picCounter;
        if (m_picCounter >= 1) {
            // Important: set the user id to 2 after the first cycle
            m_userId = 2;
        }

        if (m_picCounter > 3) {
            // We are done, we sent the pic 3 times successfully.
            setState(StateRefreshData);
            return;
        }
    } else if (m_state == StateRefreshKey) {
        // Key has been refreshed successfully...continue with data polling
        setState(StateRefreshData);
        m_refreshTimer.stop();
        m_refreshTimer.start();
        refresh();
        return;
    }
}

void KacoClient::sendInverterRequest()
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::LittleEndian);

    // The list of vectis meter property hashes
    foreach (const QString &vectisProperty, m_vectisProperties) {
        stream << static_cast<quint16>(calculateStringHashCode(vectisProperty) & 0xffff);
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

    // The list of system info hashes
    foreach (const QString &systemInfo, m_systemInfoProperties) {
        stream << static_cast<quint16>(calculateStringHashCode(systemInfo) & 0xffff);
    }

    sendData(buildPackage(MessageTypeIds, payload));
}

void KacoClient::processInverterResponse(const QByteArray &message)
{
    qCDebug(dcKacoBh10()) << "--> Process inverter data...";

    if (m_authorization != true) {
        m_authorization = true;
        emit authorizationChanged(true);
    }

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

    qCDebug(dcKacoBh10()) << "Message start:" << responseStart << byteArrayToHexString(message.left(2));
    qCDebug(dcKacoBh10()) << "Message type:" << static_cast<MessageType>(messageType) << byteToHexString(messageType);
    qCDebug(dcKacoBh10()) << "Message size:" << messageSize << byteArrayToHexString(message.mid(3,2));
    qCDebug(dcKacoBh10()) << "Message checksum:" << calculateChecksum(message.mid(9, message.length() - 9)) << "=" << checksum;

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

    while (!stream.atEnd()) {
        // Read the next param size and hash
        stream >> paramSize >> paramHash;
        //qCDebug(dcKacoBh10()) << "--> Read parameter" << paramHash << "size" << paramSize;

        // Meter ----------------------------------------

        if (paramHash == m_propertyHashes.key("dd.e_inverter_inj")) {

            // Meter feed in inverter
            stream >> paramValueRaw;
            float meterInverterEnergyReturnedToday = convertEnergyToFloat(paramValueRaw, 16, 240000.0) / 1000.0;
            qCDebug(dcKacoBh10()) << "Meter inverter feed in today" << meterInverterEnergyReturnedToday << "kWh";
            if (!qFuzzyCompare(m_meterInverterEnergyReturnedToday, meterInverterEnergyReturnedToday)) {
                m_meterInverterEnergyReturnedToday = meterInverterEnergyReturnedToday;
                emit meterInverterEnergyReturnedTodayChanged(m_meterInverterEnergyReturnedToday);
            }

            stream >> paramValueRaw;
            float meterInverterEnergyReturnedMonth = convertEnergyToFloat(paramValueRaw, 16, 7200000.0) / 1000.0;
            qCDebug(dcKacoBh10()) << "Meter inverter feed in this month" << meterInverterEnergyReturnedMonth << "kWh";
            if (!qFuzzyCompare(m_meterInverterEnergyReturnedMonth, meterInverterEnergyReturnedMonth)) {
                m_meterInverterEnergyReturnedMonth = meterInverterEnergyReturnedMonth;
                emit meterInverterEnergyReturnedMonthChanged(m_meterInverterEnergyReturnedMonth);
            }

            stream >> paramValueRaw;
            float meterInverterEnergyReturnedTotal = convertEnergyToFloat(paramValueRaw, 32, 8.76E7) / 1000.0;
            qCDebug(dcKacoBh10()) << "Meter inverter feed in total" << meterInverterEnergyReturnedTotal << "kWh";
            if (!qFuzzyCompare(m_meterInverterEnergyReturnedTotal, meterInverterEnergyReturnedTotal)) {
                m_meterInverterEnergyReturnedTotal = meterInverterEnergyReturnedTotal;
                emit meterInverterEnergyReturnedTotalChanged(m_meterInverterEnergyReturnedTotal);
            }

        } else if (paramHash == m_propertyHashes.key("dd.e_inv_feedin")) {   // Test

            // Meter consumed inverter
            stream >> paramValueRaw;
            float meterInverterEnergyReturnedToday = convertEnergyToFloat(paramValueRaw, 16, 240000.0)/ 1000.0;
            qCDebug(dcKacoBh10()) << "Meter inverter consumed today (test e_inv_feedin)" << meterInverterEnergyReturnedToday << "kWh";

            stream >> paramValueRaw;
            float meterInverterEnergyReturnedMonth = convertEnergyToFloat(paramValueRaw, 16, 7200000.0) / 1000.0;
            qCDebug(dcKacoBh10()) << "Meter inverter consumed this month (test e_inv_feedin)" << meterInverterEnergyReturnedMonth << "kWh";

            stream >> paramValueRaw;
            float meterInverterEnergyReturnedTotal = convertEnergyToFloat(paramValueRaw, 32, 8.76E7) / 1000.0;
            qCDebug(dcKacoBh10()) << "Meter inverter consumed total (test e_inv_feedin)" << meterInverterEnergyReturnedTotal << "kWh";

        } else if (paramHash == m_propertyHashes.key("dd.e_inverter_cons")) {

            // Meter consumed inverter
            stream >> paramValueRaw;
            float meterInverterEnergyConsumedToday = convertEnergyToFloat(paramValueRaw, 16, 240000.0)/ 1000.0;
            qCDebug(dcKacoBh10()) << "Meter inverter consumed today" << meterInverterEnergyConsumedToday << "kWh";
            if (!qFuzzyCompare(m_meterInverterEnergyConsumedToday, meterInverterEnergyConsumedToday)) {
                m_meterInverterEnergyConsumedToday = meterInverterEnergyConsumedToday;
                emit meterInverterEnergyConsumedTodayChanged(m_meterInverterEnergyConsumedToday);
            }

            stream >> paramValueRaw;
            float meterInverterEnergyConsumedMonth = convertEnergyToFloat(paramValueRaw, 16, 7200000.0) / 1000.0;
            qCDebug(dcKacoBh10()) << "Meter inverter consumed this month" << meterInverterEnergyConsumedMonth << "kWh";
            if (!qFuzzyCompare(m_meterInverterEnergyConsumedMonth, meterInverterEnergyConsumedMonth)) {
                m_meterInverterEnergyConsumedMonth = meterInverterEnergyConsumedMonth;
                emit meterInverterEnergyConsumedMonthChanged(m_meterInverterEnergyConsumedMonth);
            }

            stream >> paramValueRaw;
            float meterInverterEnergyConsumedTotal = convertEnergyToFloat(paramValueRaw, 32, 8.76E7) / 1000.0;
            qCDebug(dcKacoBh10()) << "Meter inverter consumed total" << meterInverterEnergyConsumedTotal << "kWh";
            if (!qFuzzyCompare(m_meterInverterEnergyConsumedTotal, meterInverterEnergyConsumedTotal)) {
                m_meterInverterEnergyConsumedTotal = meterInverterEnergyConsumedTotal;
                emit meterInverterEnergyConsumedTotalChanged(m_meterInverterEnergyConsumedTotal);
            }

        } else if (paramHash == m_propertyHashes.key("dd.e_inv_cons")) {    // Test

            // Meter consumed inverter
            stream >> paramValueRaw;
            float meterInverterEnergyConsumedToday = convertEnergyToFloat(paramValueRaw, 16, 240000.0)/ 1000.0;
            qCDebug(dcKacoBh10()) << "Meter inverter consumed today (test e_inv_cons)" << meterInverterEnergyConsumedToday << "kWh";

            stream >> paramValueRaw;
            float meterInverterEnergyConsumedMonth = convertEnergyToFloat(paramValueRaw, 16, 7200000.0) / 1000.0;
            qCDebug(dcKacoBh10()) << "Meter inverter consumed this month (test e_inv_cons)" << meterInverterEnergyConsumedMonth << "kWh";

            stream >> paramValueRaw;
            float meterInverterEnergyConsumedTotal = convertEnergyToFloat(paramValueRaw, 32, 8.76E7) / 1000.0;
            qCDebug(dcKacoBh10()) << "Meter inverter consumed total (test e_inv_cons)" << meterInverterEnergyConsumedTotal << "kWh";

        } else if (paramHash == m_propertyHashes.key("dd.e_grid_inj")) {

            // Meter grid feed in
            stream >> paramValueRaw;
            float meterGridEnergyReturnedToday = convertEnergyToFloat(paramValueRaw, 16, 2400000.0) / 1000.0;
            qCDebug(dcKacoBh10()) << "Meter grid feed in today" << meterGridEnergyReturnedToday << "kWh";
            if (!qFuzzyCompare(m_meterGridEnergyReturnedToday, meterGridEnergyReturnedToday)) {
                m_meterGridEnergyReturnedToday = meterGridEnergyReturnedToday;
                emit meterGridEnergyReturnedTodayChanged(m_meterGridEnergyReturnedToday);
            }

            stream >> paramValueRaw;
            float meterGridEnergyReturnedMonth = convertEnergyToFloat(paramValueRaw, 16, 7.2E7) / 1000.0;
            qCDebug(dcKacoBh10()) << "Meter grid feed in this month" << meterGridEnergyReturnedMonth << "kWh";
            if (!qFuzzyCompare(m_meterGridEnergyReturnedMonth, meterGridEnergyReturnedMonth)) {
                m_meterGridEnergyReturnedMonth = meterGridEnergyReturnedMonth;
                emit meterGridEnergyReturnedMonthChanged(m_meterGridEnergyReturnedMonth);
            }

            stream >> paramValueRaw;
            // Note: for some reason this value was wrong by factor 10
            float meterGridEnergyReturnedTotal = convertEnergyToFloat(paramValueRaw, 32, 8.76E7) / 100.0;
            qCDebug(dcKacoBh10()) << "Meter grid feed in total" << meterGridEnergyReturnedTotal << "kWh";
            if (!qFuzzyCompare(m_meterGridEnergyReturnedTotal, meterGridEnergyReturnedTotal)) {
                m_meterGridEnergyReturnedTotal = meterGridEnergyReturnedTotal;
                emit meterGridEnergyReturnedTotalChanged(m_meterGridEnergyReturnedTotal);
            }

        } else if (paramHash == m_propertyHashes.key("dd.e_grid_feedin")) {    // Test

            // Meter grid feed in
            stream >> paramValueRaw;
            float meterGridEnergyReturnedToday = convertEnergyToFloat(paramValueRaw, 16, 2400000.0) / 1000.0;
            qCDebug(dcKacoBh10()) << "Meter grid feed in today (test e_grid_feedin)" << meterGridEnergyReturnedToday << "kWh";

            stream >> paramValueRaw;
            float meterGridEnergyReturnedMonth = convertEnergyToFloat(paramValueRaw, 16, 7.2E7) / 1000.0;
            qCDebug(dcKacoBh10()) << "Meter grid feed in this month (test e_grid_feedin)" << meterGridEnergyReturnedMonth << "kWh";

            stream >> paramValueRaw;
            // Note: for some reason this value was wrong by factor 10
            float meterGridEnergyReturnedTotal = convertEnergyToFloat(paramValueRaw, 32, 8.76E7) / 100.0;
            qCDebug(dcKacoBh10()) << "Meter grid feed in total (test e_grid_feedin)" << meterGridEnergyReturnedTotal << "kWh";

        } else if (paramHash == m_propertyHashes.key("dd.e_grid_cons")) {

            // Meter consumed grid
            stream >> paramValueRaw;
            float meterGridEnergyConsumedToday = convertEnergyToFloat(paramValueRaw, 16, 2400000.0) / 1000.0;
            qCDebug(dcKacoBh10()) << "Meter grid consumed today" << meterGridEnergyConsumedToday << "kWh";
            if (!qFuzzyCompare(m_meterGridEnergyConsumedToday, meterGridEnergyConsumedToday)) {
                m_meterGridEnergyConsumedToday = meterGridEnergyConsumedToday;
                emit meterGridEnergyConsumedTodayChanged(m_meterGridEnergyConsumedToday);
            }

            stream >> paramValueRaw;
            float meterGridEnergyConsumedMonth = convertEnergyToFloat(paramValueRaw, 16, 7.2E7) / 1000.0;
            qCDebug(dcKacoBh10()) << "Meter grid consumed this month" << meterGridEnergyConsumedMonth << "kWh";
            if (!qFuzzyCompare(m_meterGridEnergyConsumedMonth, meterGridEnergyConsumedMonth)) {
                m_meterGridEnergyConsumedMonth = meterGridEnergyConsumedMonth;
                emit meterGridEnergyConsumedMonthChanged(m_meterGridEnergyConsumedMonth);
            }

            stream >> paramValueRaw;
            // Note: for some reason this value was wrong by factor 10
            float meterGridEnergyConsumedTotal = convertEnergyToFloat(paramValueRaw, 32, 8.76E7) / 100.0;
            qCDebug(dcKacoBh10()) << "Meter grid consumed total" << meterGridEnergyConsumedTotal << "kWh";
            if (!qFuzzyCompare(m_meterGridEnergyConsumedTotal, meterGridEnergyConsumedTotal)) {
                m_meterGridEnergyConsumedTotal = meterGridEnergyConsumedTotal;
                emit meterGridEnergyConsumedTotalChanged(m_meterGridEnergyConsumedTotal);
            }

        } else if (paramHash == m_propertyHashes.key("dd.e_compensation")) {

            // Meter self consumption
            stream >> paramValueRaw;
            float meterSelfConsumptionDay = convertEnergyToFloat(paramValueRaw, 16, 240000.0) / 1000.0;
            qCDebug(dcKacoBh10()) << "Meter self consumed today" << meterSelfConsumptionDay << "kWh";
            if (!qFuzzyCompare(m_meterSelfConsumptionDay, meterSelfConsumptionDay)) {
                m_meterSelfConsumptionDay = meterSelfConsumptionDay;
                emit meterSelfConsumptionDayChanged(m_meterSelfConsumptionDay);
            }

            stream >> paramValueRaw;
            float meterSelfConsumptionMonth = convertEnergyToFloat(paramValueRaw, 16, 7200000.0) / 1000.0;
            qCDebug(dcKacoBh10()) << "Meter self consumed this month" << meterSelfConsumptionMonth << "kWh";
            if (!qFuzzyCompare(m_meterSelfConsumptionMonth, meterSelfConsumptionMonth)) {
                m_meterSelfConsumptionMonth = meterSelfConsumptionMonth;
                emit meterSelfConsumptionMonthChanged(m_meterSelfConsumptionMonth);
            }

            stream >> paramValueRaw;
            float meterSelfConsumptionTotal = convertEnergyToFloat(paramValueRaw, 32, 8.76E7) / 1000.0;
            qCDebug(dcKacoBh10()) << "Meter self consumed total" << meterSelfConsumptionTotal << "kWh";
            if (!qFuzzyCompare(m_meterSelfConsumptionTotal, meterSelfConsumptionTotal)) {
                m_meterSelfConsumptionTotal = meterSelfConsumptionTotal;
                emit meterSelfConsumptionTotalChanged(m_meterSelfConsumptionTotal);
            }

        } else if (paramHash == m_propertyHashes.key("dd.q_acc")) {

            // Meter AH battery
            stream >> paramValueRaw;
            float meterAhBatteryToday = convertEnergyToFloat(paramValueRaw, 16, 600.0);
            qCDebug(dcKacoBh10()) << "Meter Ah battery today" << meterAhBatteryToday << "Ah";
            if (!qFuzzyCompare(m_meterAhBatteryToday, meterAhBatteryToday)) {
                m_meterAhBatteryToday = meterAhBatteryToday;
                emit meterAhBatteryTodayChanged(m_meterAhBatteryToday);
            }

            stream >> paramValueRaw;
            float meterAhBatteryMonth = convertEnergyToFloat(paramValueRaw, 16, 18000.0);
            qCDebug(dcKacoBh10()) << "Meter Ah battery this month" << meterAhBatteryMonth << "Ah";
            if (!qFuzzyCompare(m_meterAhBatteryMonth, meterAhBatteryMonth)) {
                m_meterAhBatteryMonth = meterAhBatteryMonth;
                emit meterAhBatteryMonthChanged(m_meterAhBatteryMonth);
            }

            stream >> paramValueRaw;
            float meterAhBatteryTotal = convertEnergyToFloat(paramValueRaw, 32, 219000.0);
            qCDebug(dcKacoBh10()) << "Meter Ah battery total" << meterAhBatteryTotal << "Ah";
            if (!qFuzzyCompare(m_meterAhBatteryTotal, meterAhBatteryTotal)) {
                m_meterAhBatteryTotal = meterAhBatteryTotal;
                emit meterAhBatteryTotalChanged(m_meterAhBatteryTotal);
            }




            // Inverter ----------------------------------------

        } else if (paramHash == m_propertyHashes.key("g_sync.u_l_rms[0]")) {

            // Grid voltage L1
            stream >> paramValueRaw;
            float inverterGridVoltagePhaseA = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Inverter grid voltage phase A" << inverterGridVoltagePhaseA << "V";
            if (!qFuzzyCompare(m_inverterGridVoltagePhaseA, inverterGridVoltagePhaseA)) {
                m_inverterGridVoltagePhaseA = inverterGridVoltagePhaseA;
                emit inverterGridVoltagePhaseAChanged(m_inverterGridVoltagePhaseA);
            }

        } else if (paramHash == m_propertyHashes.key("g_sync.u_l_rms[1]")) {

            // Grid voltage L2
            stream >> paramValueRaw;
            float inverterGridVoltagePhaseB = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Inverter grid voltage phase B" << inverterGridVoltagePhaseB << "V";
            if (!qFuzzyCompare(m_inverterGridVoltagePhaseB, inverterGridVoltagePhaseB)) {
                m_inverterGridVoltagePhaseB = inverterGridVoltagePhaseB;
                emit inverterGridVoltagePhaseBChanged(m_inverterGridVoltagePhaseB);
            }

        } else if (paramHash == m_propertyHashes.key("g_sync.u_l_rms[2]")) {

            // Grid voltage L3
            stream >> paramValueRaw;
            float inverterGridVoltagePhaseC = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Inverter grid voltage phase C" << inverterGridVoltagePhaseC << "V";
            if (!qFuzzyCompare(m_inverterGridVoltagePhaseC, inverterGridVoltagePhaseC)) {
                m_inverterGridVoltagePhaseC = inverterGridVoltagePhaseC;
                emit inverterGridVoltagePhaseCChanged(m_inverterGridVoltagePhaseC);
            }

        } else if (paramHash == m_propertyHashes.key("g_sync.p_ac[0]")) {

            // AC Power L1
            stream >> paramValueRaw;
            float inverterPowerPhaseA = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Inverter power phase A" << inverterPowerPhaseA << "W";
            if (!qFuzzyCompare(m_inverterPowerPhaseA, inverterPowerPhaseA)) {
                m_inverterPowerPhaseA = inverterPowerPhaseA;
                emit inverterPowerPhaseAChanged(m_inverterPowerPhaseA);
            }

        } else if (paramHash == m_propertyHashes.key("g_sync.p_ac[1]")) {

            // AC Power L2
            stream >> paramValueRaw;
            float inverterPowerPhaseB = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Inverter power phase B" << inverterPowerPhaseB << "W";
            if (!qFuzzyCompare(m_inverterPowerPhaseB, inverterPowerPhaseB)) {
                m_inverterPowerPhaseB = inverterPowerPhaseB;
                emit inverterPowerPhaseBChanged(m_inverterPowerPhaseB);
            }

        } else if (paramHash == m_propertyHashes.key("g_sync.p_ac[2]")) {

            // AC Power L3
            stream >> paramValueRaw;
            float inverterPowerPhaseC = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Inverter power phase C" << inverterPowerPhaseC << "W";
            if (!qFuzzyCompare(m_inverterPowerPhaseC, inverterPowerPhaseC)) {
                m_inverterPowerPhaseC = inverterPowerPhaseC;
                emit inverterPowerPhaseCChanged(m_inverterPowerPhaseC);
            }

        } else if (paramHash == m_propertyHashes.key("g_sync.q_ac")) {

            // AC reactive Power L1
            stream >> paramValueRaw;
            float inverterReactivePowerPhaseA = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Inverter reactive power phase A" << inverterReactivePowerPhaseA << "Var";
            if (!qFuzzyCompare(m_inverterReactivePowerPhaseA, inverterReactivePowerPhaseA)) {
                m_inverterReactivePowerPhaseA = inverterReactivePowerPhaseA;
                emit inverterReactivePowerPhaseAChanged(m_inverterReactivePowerPhaseA);
            }

            // AC reactive Power L2
            stream >> paramValueRaw;
            float inverterReactivePowerPhaseB = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Inverter reactive power phase B" << inverterReactivePowerPhaseB << "Var";
            if (!qFuzzyCompare(m_inverterReactivePowerPhaseB, inverterReactivePowerPhaseB)) {
                m_inverterReactivePowerPhaseB = inverterReactivePowerPhaseB;
                emit inverterReactivePowerPhaseBChanged(m_inverterReactivePowerPhaseB);
            }

            // AC reactive Power L2
            stream >> paramValueRaw;
            float inverterReactivePowerPhaseC = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Inverter reactive power phase C" << inverterReactivePowerPhaseC << "Var";
            if (!qFuzzyCompare(m_inverterReactivePowerPhaseC, inverterReactivePowerPhaseC)) {
                m_inverterReactivePowerPhaseC = inverterReactivePowerPhaseC;
                emit inverterReactivePowerPhaseCChanged(m_inverterReactivePowerPhaseC);
            }

        } else if (paramHash == m_propertyHashes.key("g_sync.u_sg_avg[0]")) {

            // PV voltage 1
            stream >> paramValueRaw;
            float inverterPvVoltage1 = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Inverter PV Voltage 1" << inverterPvVoltage1 << "V";
            if (!qFuzzyCompare(m_inverterPvVoltage1, inverterPvVoltage1)) {
                m_inverterPvVoltage1 = inverterPvVoltage1;
                emit inverterPvVoltage1Changed(m_inverterPvVoltage1);
            }

        } else if (paramHash == m_propertyHashes.key("g_sync.u_sg_avg[1]")) {

            // PV voltage 2
            stream >> paramValueRaw;
            float inverterPvVoltage2 = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Inverter PV Voltage 2" << inverterPvVoltage2 << "V";
            if (!qFuzzyCompare(m_inverterPvVoltage2, inverterPvVoltage2)) {
                m_inverterPvVoltage2 = inverterPvVoltage2;
                emit inverterPvVoltage2Changed(m_inverterPvVoltage2);
            }


        } else if (paramHash == m_propertyHashes.key("g_sync.p_pv_lp")) {

            // PV power
            stream >> paramValueRaw;
            float inverterPvPower = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Inverter PV power" << inverterPvPower << "W";
            if (!qFuzzyCompare(m_inverterPvPower, inverterPvPower)) {
                m_inverterPvPower = inverterPvPower;
                emit inverterPvPowerChanged(m_inverterPvPower);
            }

        } else if (paramHash == m_propertyHashes.key("gd[0].f_l_slow")) {

            // Grid frequency
            stream >> paramValueRaw;
            float inverterFrequency = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Inverter grid frequency" << inverterFrequency << "Hz";
            if (!qFuzzyCompare(m_inverterFrequency, inverterFrequency)) {
                m_inverterFrequency = inverterFrequency;
                emit inverterFrequencyChanged(m_inverterFrequency);
            }

        } else if (paramHash == m_propertyHashes.key("iso.r")) {

            // R isolation
            stream >> paramValueRaw;
            float inverterResistanceIsolation = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Inverter isolation resistence" << inverterResistanceIsolation << "Ohm";
            if (!qFuzzyCompare(m_inverterResistanceIsolation, inverterResistanceIsolation)) {
                m_inverterResistanceIsolation = inverterResistanceIsolation;
                emit inverterResistanceIsolationChanged(m_inverterResistanceIsolation);
            }


            // Battery ----------------------------------------

        } else if (paramHash == m_propertyHashes.key("g_sync.p_accu")) {

            stream >> paramValueRaw;
            float batteryPower = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Battery power" << batteryPower << "W";
            if (!qFuzzyCompare(m_batteryPower, batteryPower)) {
                m_batteryPower = batteryPower;
                emit batteryPowerChanged(m_batteryPower);
            }

        } else if (paramHash == m_propertyHashes.key("bms.u_total")) {

            stream >> paramValueRaw;
            float batteryVoltage = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Battery voltage" << batteryVoltage << "V";
            if (!qFuzzyCompare(m_batteryVoltage, batteryVoltage)) {
                m_batteryVoltage = batteryVoltage;
                emit batteryVoltageChanged(m_batteryVoltage);
            }

        } else if (paramHash == m_propertyHashes.key("bms.SOEpercent_total")) {

            stream >> paramValueRaw;
            float batteryPercentage = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Battery SOE" << batteryPercentage << "%";
            if (!qFuzzyCompare(m_batteryPercentage, batteryPercentage)) {
                m_batteryPercentage = batteryPercentage;
                emit batteryPercentageChanged(m_batteryPercentage);
            }

            // Vectis ----------------------------------------

        } else if (paramHash == m_propertyHashes.key("rs.p_ext")) {

            stream >> paramValueRaw;
            float meterPowerPhaseA = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Meter power phase A" << meterPowerPhaseA << "W";
            if (!qFuzzyCompare(m_meterPowerPhaseA, meterPowerPhaseA)) {
                m_meterPowerPhaseA = meterPowerPhaseA;
                emit meterPowerPhaseAChanged(m_meterPowerPhaseA);
            }

            stream >> paramValueRaw;
            float meterPowerPhaseB = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Meter power phase B" << meterPowerPhaseB << "W";
            if (!qFuzzyCompare(m_meterPowerPhaseB, meterPowerPhaseB)) {
                m_meterPowerPhaseB = meterPowerPhaseB;
                emit meterPowerPhaseBChanged(m_meterPowerPhaseB);
            }

            stream >> paramValueRaw;
            float meterPowerPhaseC = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Meter power phase C" << meterPowerPhaseC << "W";
            if (!qFuzzyCompare(m_meterPowerPhaseC, meterPowerPhaseC)) {
                m_meterPowerPhaseC = meterPowerPhaseC;
                emit meterPowerPhaseAChanged(m_meterPowerPhaseC);
            }

        } else if (paramHash == m_propertyHashes.key("rs.p_int")) {

            stream >> paramValueRaw;
            float meterPowerInternalPhaseA = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Meter power internal phase A" << meterPowerInternalPhaseA << "W";
            if (!qFuzzyCompare(m_meterPowerInternalPhaseA, meterPowerInternalPhaseA)) {
                m_meterPowerInternalPhaseA = meterPowerInternalPhaseA;
                emit meterPowerInternalPhaseAChanged(m_meterPowerInternalPhaseA);
            }

            stream >> paramValueRaw;
            float meterPowerInternalPhaseB = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Meter power internal phase B" << meterPowerInternalPhaseB << "W";
            if (!qFuzzyCompare(m_meterPowerInternalPhaseB, meterPowerInternalPhaseB)) {
                m_meterPowerInternalPhaseB = meterPowerInternalPhaseB;
                emit meterPowerInternalPhaseBChanged(m_meterPowerInternalPhaseB);
            }

            stream >> paramValueRaw;
            float meterPowerInternalPhaseC = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Meter power internal phase C" << meterPowerInternalPhaseC << "W";
            if (!qFuzzyCompare(m_meterPowerInternalPhaseC, meterPowerInternalPhaseC)) {
                m_meterPowerInternalPhaseC = meterPowerInternalPhaseC;
                emit meterPowerInternalPhaseAChanged(m_meterPowerInternalPhaseC);
            }

        } else if (paramHash == m_propertyHashes.key("rs.u_ext")) {

            stream >> paramValueRaw;
            float meterVoltagePhaseA = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Meter voltage phase A" << meterVoltagePhaseA << "W";
            if (!qFuzzyCompare(m_meterVoltagePhaseA, meterVoltagePhaseA)) {
                m_meterVoltagePhaseA = meterVoltagePhaseA;
                emit meterVoltagePhaseAChanged(m_meterVoltagePhaseA);
            }

            stream >> paramValueRaw;
            float meterVoltagePhaseB = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Meter voltage phase B" << meterVoltagePhaseB << "W";
            if (!qFuzzyCompare(m_meterVoltagePhaseB, meterVoltagePhaseB)) {
                m_meterVoltagePhaseB = meterVoltagePhaseB;
                emit meterVoltagePhaseBChanged(m_meterVoltagePhaseB);
            }

            stream >> paramValueRaw;
            float meterVoltagePhaseC = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Meter voltage phase C" << meterVoltagePhaseC << "W";
            if (!qFuzzyCompare(m_meterVoltagePhaseC, meterVoltagePhaseC)) {
                m_meterVoltagePhaseC = meterVoltagePhaseC;
                emit meterVoltagePhaseAChanged(m_meterVoltagePhaseC);
            }

        } else if (paramHash == m_propertyHashes.key("rs.f_ext")) {

            stream >> paramValueRaw;
            float meterFrequency = convertRawValueToFloat(paramValueRaw);
            qCDebug(dcKacoBh10()) << "Meter frequency 1" << meterFrequency << "Hz";
            if (!qFuzzyCompare(m_meterFrequency, meterFrequency)) {
                m_meterFrequency = meterFrequency;
                emit meterFrequencyChanged(m_meterFrequency);
            }

            // Unneeded properties
            stream >> paramValueRaw;
            stream >> paramValueRaw;

            // System info ----------------------------------------

        } else if (paramHash == m_propertyHashes.key("sw_version")) {

            quint8 versionMinor;
            quint8 versionMajor;
            stream >> versionMinor;
            stream >> versionMajor;
            QString controllerVersion{QString("%1.%2").arg(versionMajor).arg(versionMinor)};
            qCDebug(dcKacoBh10()) << "Controller version" << controllerVersion;
            if (m_controllerVersion != controllerVersion) {
                m_controllerVersion = controllerVersion;
                emit controllerVersionChanged(m_controllerVersion);
            }

        } else if (paramHash == m_propertyHashes.key("rs.db_version")) {

            quint8 versionMinor;
            quint8 versionMajor;
            stream >> versionMinor;
            stream >> versionMajor;
            QString hyswitchVersion{QString("%1.%2").arg(versionMajor).arg(versionMinor)};
            qCDebug(dcKacoBh10()) << "Hy-switch version" << hyswitchVersion;
            if (m_hyswitchVersion != hyswitchVersion) {
                m_hyswitchVersion = hyswitchVersion;
                emit hyswitchVersionChanged(m_hyswitchVersion);
            }

        } else {
            // Unknown property hash received...let's skip and see if we find more valid data
            qCWarning(dcKacoBh10()) << "Unknown property hash. Skipping property with size" << paramSize << "and hash value" << paramHash;
            quint8 skippedByte;
            for (int i = 0; i < paramSize; i++) {
                stream >> skippedByte;
            }
        }
    }

    emit valuesUpdated();
}

QByteArray KacoClient::buildPackage(MessageType messageType, const QByteArray &payload)
{
    QByteArray package;
    QDataStream stream(&package, QIODevice::ReadWrite);
    stream.setByteOrder(QDataStream::LittleEndian);

    // 0, 1: Static start bytes
    stream << static_cast<quint8>(0x55);
    stream << static_cast<quint8>(0xaa);

    // 2: Message type
    stream << static_cast<quint8>(messageType);

    // 3, 4: Message length, fill in later
    stream << static_cast<quint16>(payload.length());

    // 5, 6, 7, 8: Data checksum, fill in later
    stream << static_cast<quint32>(calculateChecksum(payload));

    // The rest is payload
    for (int i = 0; i < payload.count(); i++)
        stream << static_cast<quint8>(payload.at(i));

    return package;
}

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

    //qCDebug(dcKacoBh10()) << "Hash of" << name << hashSum;
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

float KacoClient::convertEnergyToFloat(quint32 rawValue, uint offset, float scale)
{
    return static_cast<float>((static_cast<qint64>(rawValue) * scale) / static_cast<float>(static_cast<quint64>(1) << offset));
}

bool KacoClient::picRefreshRequired()
{
    quint16 currentTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
    // Unsigned rollover is used intentionally here.
    quint16 milliSecondsSinceLastRefresh = currentTime - m_lastPicTimestamp;
    qCDebug(dcKacoBh10()) << "Checking pic refresh, milliseconds since last timestamp:" << milliSecondsSinceLastRefresh;
    return (milliSecondsSinceLastRefresh) >= 5000;
}

void KacoClient::printHashCodes(const QStringList &properties)
{
    foreach (const QString &status, properties) {
        QByteArray data;
        QDataStream stream(&data, QIODevice::ReadWrite);
        stream.setByteOrder(QDataStream::LittleEndian);
        quint16 hashCode = calculateStringHashCode(status) & 0xffff;
        stream << hashCode;
        qCDebug(dcKacoBh10()) << " -" << data.toHex() << "|" << status << byteArrayToHexString(data);
    }
}

void KacoClient::refresh()
{
    if (m_requestPending) {
        m_requestPendingTicks++;
        if (m_requestPendingTicks >= 5) {
            m_requestPendingTicks = 0;
            m_requestPending = false;
            qCWarning(dcKacoBh10()) << "No response received for" << m_state << ". Retry";
            emit connectedChanged(false);
        } else {
            return;
        }
    }

    if (m_state != StateAuthenticate && picRefreshRequired()) {
        qCDebug(dcKacoBh10()) << "Refreshing key required...";
        setState(StateRefreshKey);
    }

    switch (m_state) {
    case StateNone:
        break;
    case StateAuthenticate:
    case StateRefreshKey:
        sendPicRequest();
        break;
    case StateInitialize:
        // TODO: get settings and system information, emit authenticated on success
        break;
    case StateRefreshData:
        sendInverterRequest();
        break;
    }
}






