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


#include "integrationpluginkaco.h"
#include "plugininfo.h"

#include <network/networkdevicediscovery.h>

IntegrationPluginKaco::IntegrationPluginKaco()
{

}

void IntegrationPluginKaco::discoverThings(ThingDiscoveryInfo *info)
{
    if (!hardwareManager()->networkDeviceDiscovery()->available()) {
        qCWarning(dcKaco()) << "Failed to discover network devices. The network device discovery is not available.";
        info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("Unable to discovery devices in your network."));
        return;
    }

    qCDebug(dcKaco()) << "Starting network discovery...";
    NetworkDeviceDiscoveryReply *discoveryReply = hardwareManager()->networkDeviceDiscovery()->discover();
    connect(discoveryReply, &NetworkDeviceDiscoveryReply::finished, this, [=](){
        ThingDescriptors descriptors;
        qCDebug(dcKaco()) << "Discovery finished. Found" << discoveryReply->networkDeviceInfos().count() << "devices";
        foreach (const NetworkDeviceInfo &networkDeviceInfo, discoveryReply->networkDeviceInfos()) {
            qCDebug(dcKaco()) << networkDeviceInfo;
            if (networkDeviceInfo.macAddress().isNull())
                continue;

            QString title;
            if (networkDeviceInfo.hostName().isEmpty()) {
                title += networkDeviceInfo.address().toString();
            } else {
                title += networkDeviceInfo.address().toString() + " (" + networkDeviceInfo.hostName() + ")";
            }

            QString description;
            if (networkDeviceInfo.macAddressManufacturer().isEmpty()) {
                description = networkDeviceInfo.macAddress();
            } else {
                description = networkDeviceInfo.macAddress() + " (" + networkDeviceInfo.macAddressManufacturer() + ")";
            }

            ThingDescriptor descriptor(kacoThingClassId, title, description);

            // Check if we already have set up this device
            Things existingThings = myThings().filterByParam(kacoThingMacAddressParamTypeId, networkDeviceInfo.macAddress());
            if (existingThings.count() == 1) {
                qCDebug(dcKaco()) << "This thing already exists in the system." << existingThings.first() << networkDeviceInfo;
                descriptor.setThingId(existingThings.first()->id());
            }

            ParamList params;
            params << Param(kacoThingHostAddressParamTypeId, networkDeviceInfo.address().toString());
            params << Param(kacoThingMacAddressParamTypeId, networkDeviceInfo.macAddress());
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }
        info->finish(Thing::ThingErrorNoError);
    });
}

void IntegrationPluginKaco::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcKaco()) << "Setup" << thing << thing->params();

    if (m_clients.contains(info->thing())) {
        qCDebug(dcKaco()) << "Clean up client for" << thing;
        delete m_clients.take(thing);
    }

    QHostAddress hostAddress = QHostAddress(thing->paramValue(kacoThingHostAddressParamTypeId).toString());

    KacoClient *client = new KacoClient(hostAddress, 9760, this);
    connect(client, &KacoClient::connectedChanged, thing, [=](bool connected){
        qCDebug(dcKaco()) << thing << "connected changed" << connected;
        thing->setStateValue(kacoConnectedStateTypeId, connected);

        /*

            --> (20 bytes) 55 aa 30 0b 00 3a 04 00 00 74 13 46 ad 18 a3 f8 6d 2b 75 00
            <-- (66 bytes) ed de 30 39 00 89 0b 00 00 02 00 23 4b 05 09 06 00 1a b1 68 27 19 39 26 9c 02 00 1d cf 01 02 14 00 25 a1 32 30 31 32 31 30 32 34 35 35 37 38 20 20 20 20 20 20 20 20 ca 57 c0 1f c9 9e 00 00 00 00 00

            --> (20 bytes) 55 aa 30 0b 00 13 05 00 00 7d f5 50 9f 8b 19 29 2d f6 c1 01
            <-- (66 bytes) ed de 30 39 00 46 0b 00 00 02 00 23 4b 05 09 06 00 1a b1 68 27 19 39 26 9c 02 00 1d cf 01 02 14 00 25 a1 32 30 31 32 31 30 32 34 35 35 37 38 20 20 20 20 20 20 20 20 ac 46 48 a8 b1 8e 02 00 00 01 00


          */



    });

    m_clients.insert(thing, client);
    client->connectToDevice();

    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginKaco::executeAction(ThingActionInfo *info)
{
    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginKaco::thingRemoved(Thing *thing)
{
    if (m_clients.contains(thing))
        m_clients.take(thing)->deleteLater();
}
