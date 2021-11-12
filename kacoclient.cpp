#include "kacoclient.h"
#include "extern-plugininfo.h"

KacoClient::KacoClient(const QHostAddress &hostAddress, quint16 port, QObject *parent) :
    QObject(parent),
    m_hostAddress(hostAddress),
    m_port(port)
{
    qCDebug(dcKaco()) << m_privateKeyBase64;
    m_privateKey = QByteArray::fromBase64(m_privateKeyBase64.toUtf8());
    qCDebug(dcKaco()) << m_privateKey;
    qCDebug(dcKaco()) << m_privateKey.toHex();
    qCDebug(dcKaco()) << QString::fromUtf8(m_privateKey);


    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, [=](){
        qCDebug(dcKaco()) << "Connected to" << QString("%1:%2").arg(m_hostAddress.toString(), m_port);
        emit connectedChanged(true);
    });

    connect(m_socket, &QTcpSocket::disconnected, this, [=](){
        qCDebug(dcKaco()) << "Disconnected from" << QString("%1:%2").arg(m_hostAddress.toString(), m_port) << m_socket->errorString();
        emit connectedChanged(false);
    });

    connect(m_socket, &QTcpSocket::readyRead, this, [=](){
        QByteArray data = m_socket->readAll();
        qCDebug(dcKaco()) << "Data from" << QString("%1:%2").arg(m_hostAddress.toString(), m_port);
        qCDebug(dcKaco()) << "---------------------------------------";
        qCDebug(dcKaco()) << qUtf8Printable(data);
        qCDebug(dcKaco()) << "---------------------------------------";
        qCDebug(dcKaco()) << qUtf8Printable(data.toHex());
        qCDebug(dcKaco()) << "---------------------------------------";
    });

    connect(m_socket, &QTcpSocket::stateChanged, this, [=](QAbstractSocket::SocketState socketState){
        qCDebug(dcKaco()) << "Socket state changed" << QString("%1:%2").arg(m_hostAddress.toString(), m_port) << socketState;
    });
}

bool KacoClient::connected() const
{
    return (m_socket->state() == QAbstractSocket::ConnectedState);
}

void KacoClient::connectToDevice()
{
    m_socket->connectToHost(m_hostAddress.toString(), m_port);
}

void KacoClient::disconnectFromDevice()
{
    m_socket->close();
}

bool KacoClient::sendData(const QByteArray &data)
{
    if (!connected())
        return false;

    int writtenBytes = m_socket->write(data);
    return (writtenBytes == data.count());
}
