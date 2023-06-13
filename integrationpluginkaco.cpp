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

void IntegrationPluginKaco::init()
{

}

void IntegrationPluginKaco::discoverThings(ThingDiscoveryInfo *info)
{
    KacoDiscovery *discovery = new KacoDiscovery(this);
    connect(discovery, &KacoDiscovery::discoveryFinished, this, [=](){
        qCDebug(dcKaco()) << "Discovery finished. Found" << discovery->result().count() << "inverters.";
        foreach (const KacoDiscovery::KacoDicoveryResult &result, discovery->result()) {
            QString title = "Kaco Inverter";
            QString description = "Serial: " + result.serialNumber + " (" + result.hostAddress.toString() + ")";
            ThingDescriptor descriptor(inverterThingClassId, title, description);

            // Check if we already have set up this device
            Things existingThings = myThings().filterByParam(inverterThingSerialNumberParamTypeId, result.serialNumber);
            if (existingThings.count() == 1) {
                qCDebug(dcKaco()) << "This thing already exists in the system." << existingThings.first() << result.serialNumber;
                descriptor.setThingId(existingThings.first()->id());
            }

            ParamList params;
            params << Param(inverterThingHostAddressParamTypeId, result.hostAddress.toString());
            params << Param(inverterThingPortParamTypeId, result.servicePort);
            params << Param(inverterThingMacAddressParamTypeId, result.mac);
            params << Param(inverterThingSerialNumberParamTypeId, result.serialNumber);
            descriptor.setParams(params);
            info->addThingDescriptor(descriptor);
        }

        info->finish(Thing::ThingErrorNoError);
        discovery->deleteLater();
    });

    discovery->startDiscovery();
}

void IntegrationPluginKaco::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcKaco()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == inverterThingClassId) {
        if (m_clients.contains(info->thing())) {
            qCDebug(dcKaco()) << "Clean up client for" << thing;
            delete m_clients.take(thing);
        }

        QHostAddress hostAddress = QHostAddress(thing->paramValue(inverterThingHostAddressParamTypeId).toString());
        quint16 port = thing->paramValue(inverterThingPortParamTypeId).toUInt();
        KacoClient *client = new KacoClient(hostAddress, port, "user", this);
        connect(client, &KacoClient::connectedChanged, thing, [=](bool connected){
            qCDebug(dcKaco()) << thing << "connected changed" << connected;
            if (thing->stateValue(inverterConnectedStateTypeId).toBool() != connected) {
                thing->setStateValue(inverterConnectedStateTypeId, connected);

                // Update all child devices
                foreach (Thing *thing, myThings().filterByParentId(thing->id())) {
                    thing->setStateValue("connected", connected);
                }
            }
        });

        connect(client, &KacoClient::firmwareVersionChanged, thing, [=](QString firmwareVersion){
            thing->setStateValue(inverterFirmwareVersionStateTypeId, firmwareVersion);
        });

        connect(client, &KacoClient::inverterPvPowerChanged, thing, [=](float inverterPvPower){
            thing->setStateValue(inverterCurrentPowerStateTypeId, -inverterPvPower);
        });

        connect(client, &KacoClient::inverterPvVoltage1Changed, thing, [=](float inverterPvVoltage1){
            thing->setStateValue(inverterPvVoltage1StateTypeId, inverterPvVoltage1);
        });

        connect(client, &KacoClient::inverterPvVoltage2Changed, thing, [=](float inverterPvVoltage2){
            thing->setStateValue(inverterPvVoltage2StateTypeId, inverterPvVoltage2);
        });

        // Power

        connect(client, &KacoClient::inverterPowerPhaseAChanged, thing, [=](float inverterPowerPhaseA){
            thing->setStateValue(inverterCurrentPowerPhaseAStateTypeId, -inverterPowerPhaseA);
        });

        connect(client, &KacoClient::inverterPowerPhaseBChanged, thing, [=](float inverterPowerPhaseB){
            thing->setStateValue(inverterCurrentPowerPhaseBStateTypeId, -inverterPowerPhaseB);
        });

        connect(client, &KacoClient::inverterPowerPhaseCChanged, thing, [=](float inverterPowerPhaseC){
            thing->setStateValue(inverterCurrentPowerPhaseCStateTypeId, -inverterPowerPhaseC);
        });

        // Voltage

        connect(client, &KacoClient::inverterGridVoltagePhaseAChanged, thing, [=](float inverterGridVoltagePhaseA){
            thing->setStateValue(inverterVoltagePhaseAStateTypeId, inverterGridVoltagePhaseA);
        });

        connect(client, &KacoClient::inverterGridVoltagePhaseBChanged, thing, [=](float inverterGridVoltagePhaseB){
            thing->setStateValue(inverterVoltagePhaseBStateTypeId, inverterGridVoltagePhaseB);
        });

        connect(client, &KacoClient::inverterGridVoltagePhaseCChanged, thing, [=](float inverterGridVoltagePhaseC){
            thing->setStateValue(inverterVoltagePhaseCStateTypeId, inverterGridVoltagePhaseC);
        });

        connect(client, &KacoClient::inverterFrequencyChanged, thing, [=](float frequency){
            thing->setStateValue(inverterFrequencyStateTypeId, frequency);
        });

        connect(client, &KacoClient::inverterResistanceIsolationChanged, thing, [=](float inverterResistanceIsolation){
            thing->setStateValue(inverterResistanceIsolationStateTypeId, inverterResistanceIsolation);
        });

        // Energy
        connect(client, &KacoClient::valuesUpdated, thing, [=](){
            // Energy
            thing->setStateValue(inverterTotalEnergyProducedStateTypeId, client->meterInverterEnergyReturnedTotal());

            thing->setStateValue(inverterFeedBatteryTodayStateTypeId, client->meterAhBatteryToday() * client->batteryVoltage() / 1000.0);
            thing->setStateValue(inverterFeedBatteryMonthStateTypeId, client->meterAhBatteryMonth() * client->batteryVoltage() / 1000.0);
            thing->setStateValue(inverterFeedBatteryTotalStateTypeId, client->meterAhBatteryTotal() * client->batteryVoltage() / 1000.0);

        });


        // Meter. Since we need to sum up stuff, lets react on the valuesChanged signal
        connect(client, &KacoClient::valuesUpdated, thing, [=](){
            Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(meterThingClassId);
            if (!meterThings.isEmpty()) {
                meterThings.first()->setStateValue(meterCurrentPowerPhaseAStateTypeId, client->meterPowerInternalPhaseA());
                meterThings.first()->setStateValue(meterCurrentPowerPhaseBStateTypeId, client->meterPowerInternalPhaseB());
                meterThings.first()->setStateValue(meterCurrentPowerPhaseCStateTypeId, client->meterPowerInternalPhaseC());
                meterThings.first()->setStateValue(meterCurrentPowerStateTypeId, client->meterPowerInternalPhaseA() + client->meterPowerInternalPhaseB() + client->meterPowerInternalPhaseC());
                meterThings.first()->setStateValue(meterVoltagePhaseAStateTypeId, client->meterVoltagePhaseA());
                meterThings.first()->setStateValue(meterVoltagePhaseBStateTypeId, client->meterVoltagePhaseB());
                meterThings.first()->setStateValue(meterVoltagePhaseCStateTypeId, client->meterVoltagePhaseC());
                meterThings.first()->setStateValue(meterFrequencyStateTypeId, client->meterFrequency());

                // Calculate the current
                meterThings.first()->setStateValue(meterCurrentPhaseAStateTypeId, client->meterPowerInternalPhaseA() / client->meterVoltagePhaseA());
                meterThings.first()->setStateValue(meterCurrentPhaseBStateTypeId, client->meterPowerInternalPhaseB() / client->meterVoltagePhaseB());
                meterThings.first()->setStateValue(meterCurrentPhaseCStateTypeId, client->meterPowerInternalPhaseC() / client->meterVoltagePhaseC());

                // Energy
                meterThings.first()->setStateValue(meterTotalEnergyConsumedStateTypeId, client->meterGridEnergyConsumedTotal());
                meterThings.first()->setStateValue(meterTotalEnergyProducedStateTypeId, client->meterGridEnergyReturnedTotal());
            }
        });

        m_clients.insert(thing, client);
        client->connectToDevice();

        info->finish(Thing::ThingErrorNoError);
    } else if (thing->thingClassId() == meterThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue(meterConnectedStateTypeId, parentThing->stateValue(inverterConnectedStateTypeId).toBool());
        }
    } else if (thing->thingClassId() == batteryThingClassId) {
        // Get the client for this battery
        KacoClient *client = m_clients.value(myThings().findById(thing->parentId()));
        if (!client) {
            qCWarning(dcKaco()) << "Could not find kaco client for set up" << thing;
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        connect(client, &KacoClient::valuesUpdated, thing, [=](){
            // We received data, we are connected
            thing->setStateValue(batteryConnectedStateTypeId, true);

            thing->setStateValue(batteryBatteryLevelStateTypeId, client->batteryPercentage());
            thing->setStateValue(batteryBatteryCriticalStateTypeId, client->batteryPercentage() <= 10);

            // Capacity is still unknown, Lum might provide the information

            float power = client->batteryPower();
            if (power < 1 && power > -1) {
                power = 0;
            }
            thing->setStateValue(batteryCurrentPowerStateTypeId, power);
            if (power > 1) {
                thing->setStateValue(batteryChargingStateStateTypeId, "charging");
            } else if (power < -1) {
                thing->setStateValue(batteryChargingStateStateTypeId, "discharging");
            } else {
                thing->setStateValue(batteryChargingStateStateTypeId, "idle");
            }
        });

        info->finish(Thing::ThingErrorNoError);
    }
}

void IntegrationPluginKaco::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == inverterThingClassId) {
        // Check if we need to set up the child things, meter and storage
        Things childThings = myThings().filterByParentId(thing->id());
        ThingDescriptors descriptors;
        if (childThings.filterByThingClassId(meterThingClassId).isEmpty()) {
            // No meter set up yet for this inverter, lets create the child device
            qCDebug(dcKaco()) << "Setup new meter for" << thing;
            descriptors.append(ThingDescriptor(meterThingClassId, "Kaco Energy Meter", QString(), thing->id()));
        }

        if (childThings.filterByThingClassId(batteryThingClassId).isEmpty()) {
            // No battery set up yet for this inverter, lets create the child device
            qCDebug(dcKaco()) << "Setup new battery for" << thing;
            descriptors.append(ThingDescriptor(batteryThingClassId, "Kaco Battery", QString(), thing->id()));
        }

        if (!descriptors.isEmpty()) {
            emit autoThingsAppeared(descriptors);
        }
    } else if (thing->thingClassId() == meterThingClassId) {
        KacoClient *client = m_clients.value(myThings().findById(thing->parentId()));
        if (client) {
            // Set the initial connected state
            thing->setStateValue(meterConnectedStateTypeId, client->connected());
            return;
        }
    } else if (thing->thingClassId() == batteryThingClassId) {
        KacoClient *client = m_clients.value(myThings().findById(thing->parentId()));
        if (client) {
            // Set the initial connected state
            thing->setStateValue(batteryConnectedStateTypeId, client->connected());
            return;
        }
    }
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

