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
    //KacoClient client(QHostAddress::LocalHost, 9760, "%lisala99");
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
            thing->setStateValue(inverterConnectedStateTypeId, connected);

            // TODO: authenticate before finish setup

            // Update all child devices
            foreach (Thing *thing, myThings().filterByParentId(thing->id())) {
                thing->setStateValue("connected", connected);
            }
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
            double energyProduced = client->meterInverterEnergyReturnedPhaseA() + client->meterInverterEnergyReturnedPhaseB() + client->meterInverterEnergyReturnedPhaseC();
            qCDebug(dcKaco()) << "Invertr energy produced" << energyProduced << "kWh";
            thing->setStateValue(inverterTotalEnergyProducedStateTypeId, energyProduced);

            thing->setStateValue(inverterFeedBatteryTodayStateTypeId, client->meterAhBatteryPhaseA() * client->batteryVoltage() / 1000.0);
            thing->setStateValue(inverterFeedBatteryMonthStateTypeId, client->meterAhBatteryPhaseB() * client->batteryVoltage() / 1000.0);
            thing->setStateValue(inverterFeedBatteryTotalStateTypeId, client->meterAhBatteryPhaseC() * client->batteryVoltage() / 1000.0);

            // Available energy
            // Sum Ah of battery * battery voltage
//            float totalBatteryAh = client->meterAhBatteryPhaseA() + client->meterAhBatteryPhaseB() + client->meterAhBatteryPhaseC();
//            float totalEnergy = totalBatteryAh * client->batteryVoltage();

//            qCDebug(dcKaco()) << "Battery total" << totalBatteryAh << "Ah";
//            qCDebug(dcKaco()) << "Battery total energy" << totalEnergy  << "Wh" << totalEnergy / 1000.0 << "kWh";
//            thing->setStateValue(batteryCapacityStateTypeId, totalEnergy / 1000.0);


        });

        m_clients.insert(thing, client);
        client->connectToDevice();

        info->finish(Thing::ThingErrorNoError);
    } else if (thing->thingClassId() == meterThingClassId) {
        // Get the client for this meter
        KacoClient *client = m_clients.value(myThings().findById(thing->parentId()));
        if (!client) {
            qCWarning(dcKaco()) << "Could not find kaco client for set up" << thing;
            info->finish(Thing::ThingErrorHardwareNotAvailable);
            return;
        }

        // Set the initial connected state
        thing->setStateValue(meterConnectedStateTypeId, client->connected());

        // Since we need to summ up stuff, lets react on the valuesChanged signal
        connect(client, &KacoClient::valuesUpdated, thing, [=](){
            // We received data, we are connected
            thing->setStateValue(meterConnectedStateTypeId, true);

            thing->setStateValue(meterCurrentPowerPhaseAStateTypeId, client->meterPowerInternalPhaseA());
            thing->setStateValue(meterCurrentPowerPhaseBStateTypeId, client->meterPowerInternalPhaseB());
            thing->setStateValue(meterCurrentPowerPhaseCStateTypeId, client->meterPowerInternalPhaseC());
            thing->setStateValue(meterCurrentPowerStateTypeId, client->meterPowerInternalPhaseA() + client->meterPowerInternalPhaseB() + client->meterPowerInternalPhaseC());
            thing->setStateValue(meterVoltagePhaseAStateTypeId, client->meterVoltagePhaseA());
            thing->setStateValue(meterVoltagePhaseBStateTypeId, client->meterVoltagePhaseB());
            thing->setStateValue(meterVoltagePhaseCStateTypeId, client->meterVoltagePhaseC());
            thing->setStateValue(meterFrequencyStateTypeId, client->meterFrequency());

            // Calculate the current
            double currentPowerPhaseA = client->meterPowerInternalPhaseA() / client->meterVoltagePhaseA();
            double currentPowerPhaseB = client->meterPowerInternalPhaseB() / client->meterVoltagePhaseB();
            double currentPowerPhaseC = client->meterPowerInternalPhaseC() / client->meterVoltagePhaseC();
            thing->setStateValue(meterCurrentPhaseAStateTypeId, currentPowerPhaseA);
            thing->setStateValue(meterCurrentPhaseBStateTypeId, currentPowerPhaseB);
            thing->setStateValue(meterCurrentPhaseCStateTypeId, currentPowerPhaseC);

            // Energy
            double energyConsumed = client->meterGridEnergyConsumedPhaseA() + client->meterGridEnergyConsumedPhaseB() + client->meterGridEnergyConsumedPhaseC();
            qCDebug(dcKaco()) << "Meter energy consumed" << energyConsumed << "kWh";
            thing->setStateValue(meterTotalEnergyConsumedStateTypeId, energyConsumed);

            double energyProduced = client->meterGridEnergyReturnedPhaseA() + client->meterGridEnergyReturnedPhaseB() + client->meterGridEnergyReturnedPhaseC();
            qCDebug(dcKaco()) << "Meter energy returned" << energyProduced << "kWh";
            thing->setStateValue(meterTotalEnergyProducedStateTypeId, energyProduced);

        });

        info->finish(Thing::ThingErrorNoError);

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

            // Capacity is still unknown, Lum might be the solution

            float power = client->batteryPower();
            thing->setStateValue(batteryCurrentPowerStateTypeId, power);
            if (power > 0) {
                thing->setStateValue(batteryChargingStateStateTypeId, "charging");
            } else if (power < 0) {
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

