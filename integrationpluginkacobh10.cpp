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

#include "integrationpluginkacobh10.h"
#include "plugininfo.h"

#include <network/networkdevicediscovery.h>
#include <hardwaremanager.h>

IntegrationPluginKacoBh10::IntegrationPluginKacoBh10()
{

}

void IntegrationPluginKacoBh10::init()
{

}

void IntegrationPluginKacoBh10::discoverThings(ThingDiscoveryInfo *info)
{
    KacoDiscovery *discovery = new KacoDiscovery(this);
    connect(discovery, &KacoDiscovery::discoveryFinished, this, [=](){
        qCDebug(dcKacoBh10()) << "Discovery finished. Found" << discovery->result().count() << "inverters.";
        foreach (const KacoDiscovery::KacoDicoveryResult &result, discovery->result()) {
            QString title = "Kaco Inverter";
            QString description = "Serial: " + result.serialNumber + " (" + result.hostAddress.toString() + ")";
            ThingDescriptor descriptor(inverterThingClassId, title, description);

            // Check if we already have set up this device
            Things existingThings = myThings().filterByParam(inverterThingSerialNumberParamTypeId, result.serialNumber);
            if (existingThings.count() == 1) {
                qCDebug(dcKacoBh10()) << "This thing already exists in the system." << existingThings.first() << result.serialNumber;
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

void IntegrationPluginKacoBh10::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    qCDebug(dcKacoBh10()) << "Setup" << thing << thing->params();

    if (thing->thingClassId() == inverterThingClassId) {
        if (m_clients.contains(info->thing())) {
            qCDebug(dcKacoBh10()) << "Clean up client for" << thing;
            delete m_clients.take(thing);
        }
        if (m_monitors.contains(thing)) {
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
        }

        // macAddress.isValid() returns true if the address is all zeroes. But registering a monitor with an address of all zeroes 
        // will return a nullpointer. So need to test for zero mac address, to not get a null pointer monitor.
        if (!macAddress.isNull()) {
            qCWarning(dcKacoBh10()) << "The configured mac address is not valid" << thing->params();
            info->finish(Thing::ThingErrorInvalidParameter, QT_TR_NOOP("The MAC address is not known. Please reconfigure the thing."));
            return;
        }

        NetworkDeviceMonitor *monitor = hardwareManager()->networkDeviceDiscovery()->registerMonitor(macAddress);
        m_monitors.insert(thing, monitor);

        QHostAddress hostAddress = monitor->networkDeviceInfo().address();
        if (hostAddress.isNull()) {
            qCWarning(dcKacoBh10()) << "Cannot set up thing. The host address is not known yet...";
            hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("The host address is not known yet. Trying later again."));
            return;
        }

        connect(info, &ThingSetupInfo::aborted, monitor, [=](){
            if (m_monitors.contains(thing)) {
                qCDebug(dcKacoBh10()) << "Unregistering monitor because setup has been aborted.";
                hardwareManager()->networkDeviceDiscovery()->unregisterMonitor(m_monitors.take(thing));
            }
        });

        if (monitor->reachable()) {
            setupKacoClient(info);
        } else {
            qCDebug(dcKacoBh10()) << "Waiting for the network monitor to get reachable before continue to set up the connection" << thing->name() << hostAddress.toString() << "...";
            connect(monitor, &NetworkDeviceMonitor::reachableChanged, info, [=](bool reachable){
                if (reachable) {
                    qCDebug(dcKacoBh10()) << "The monitor for thing setup" << thing->name() << "is now reachable. Continue setup...";
                    setupKacoClient(info);

                    // Delete this connection, since this should only execute once.
                    disconnect(monitor, &NetworkDeviceMonitor::reachableChanged, nullptr, nullptr);
                }
            });
        }
    } else if (thing->thingClassId() == meterThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);
        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue(meterConnectedStateTypeId, parentThing->stateValue(inverterConnectedStateTypeId).toBool());
        }
    } else if (thing->thingClassId() == batteryThingClassId) {
        // Nothing to do here, we get all information from the inverter connection
        info->finish(Thing::ThingErrorNoError);

        // Set battery capacity from settings on restart.
        thing->setStateValue(batteryCapacityStateTypeId, thing->setting(batterySettingsCapacityParamTypeId).toUInt());

        // Set battery capacity on settings change.
        connect(thing, &Thing::settingChanged, this, [this, thing] (const ParamTypeId &paramTypeId, const QVariant &value) {
            if (paramTypeId == batterySettingsCapacityParamTypeId) {
                qCDebug(dcKacoBh10()) << "Battery capacity changed to" << value.toInt() << "kWh";
                thing->setStateValue(batteryCapacityStateTypeId, value.toInt());
            }
        });

        Thing *parentThing = myThings().findById(thing->parentId());
        if (parentThing) {
            thing->setStateValue(batteryConnectedStateTypeId, parentThing->stateValue(inverterConnectedStateTypeId).toBool());
        }
    }
}

void IntegrationPluginKacoBh10::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == inverterThingClassId) {
        // Check if w have to set up a child meter for this inverter connection
        if (myThings().filterByParentId(thing->id()).filterByThingClassId(meterThingClassId).isEmpty()) {
            qCDebug(dcKacoBh10()) << "Setup new meter for" << thing;
            emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(meterThingClassId, "Kaco Energy Meter", QString(), thing->id()));
        }
    }
}

void IntegrationPluginKacoBh10::executeAction(ThingActionInfo *info)
{
    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginKacoBh10::thingRemoved(Thing *thing)
{
    if (m_clients.contains(thing))
        m_clients.take(thing)->deleteLater();
}

QHostAddress IntegrationPluginKacoBh10::getHostAddress()
{
    // There can only be one inverter.
    Thing *inverterThing = myThings().filterByThingClassId(inverterThingClassId).first();
    return m_monitors.value(inverterThing)->networkDeviceInfo().address();
}

void IntegrationPluginKacoBh10::setupKacoClient(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    quint16 port = thing->paramValue(inverterThingPortParamTypeId).toUInt();
    QString userKey = thing->paramValue(inverterThingUserKeyParamTypeId).toString();
    QHostAddress hostAddress = m_monitors.value(thing)->networkDeviceInfo().address();
    KacoClient *client = new KacoClient(hostAddress, port, userKey, this);
    connect(client, &KacoClient::connectedChanged, thing, [=](bool connected){
        qCDebug(dcKacoBh10()) << thing << "connected changed" << connected;
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

    connect(client, &KacoClient::authorizationChanged, thing, [=](bool authorization){
        thing->setStateValue(inverterAuthorizationStateTypeId, authorization);
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


    // Some values require other values to calculate. React to the valuesChanged signal for that.
    connect(client, &KacoClient::valuesUpdated, thing, [=](){

        // Energy
        thing->setStateValue(inverterTotalEnergyProducedStateTypeId, client->meterInverterEnergyReturnedTotal());

        thing->setStateValue(inverterFeedBatteryTodayStateTypeId, client->meterAhBatteryToday() * client->batteryVoltage() / 1000.0);
        thing->setStateValue(inverterFeedBatteryMonthStateTypeId, client->meterAhBatteryMonth() * client->batteryVoltage() / 1000.0);
        thing->setStateValue(inverterFeedBatteryTotalStateTypeId, client->meterAhBatteryTotal() * client->batteryVoltage() / 1000.0);

        // Meter.
        Things meterThings = myThings().filterByParentId(thing->id()).filterByThingClassId(meterThingClassId);
        if (!meterThings.isEmpty()) {

            meterThings.first()->setStateValue(meterCurrentPowerPhaseAStateTypeId, client->meterPowerInternalPhaseA());
            meterThings.first()->setStateValue(meterCurrentPowerPhaseBStateTypeId, client->meterPowerInternalPhaseB());
            meterThings.first()->setStateValue(meterCurrentPowerPhaseCStateTypeId, client->meterPowerInternalPhaseC());

            double currentPower{client->meterPowerInternalPhaseA() + client->meterPowerInternalPhaseB() + client->meterPowerInternalPhaseC()};
            if (currentPower < 5 && currentPower > -5) {    // Kill small leaking currents
                currentPower = 0;
            }
            meterThings.first()->setStateValue(meterCurrentPowerStateTypeId, currentPower);

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

        // Battery. Use SoC value to detect if a battery is present.
        float batterySoc = client->batteryPercentage();
        if (batterySoc > 0.1) {
            Things batteryThings = myThings().filterByParentId(thing->id()).filterByThingClassId(batteryThingClassId);

            if (batteryThings.isEmpty()) {
                // No battery set up yet. Need to create it.
                qCDebug(dcKacoBh10()) << "Setup new battery for" << thing;
                emit autoThingsAppeared(ThingDescriptors() << ThingDescriptor(batteryThingClassId, "Kaco Battery", QString(), thing->id()));
            } else {
                batteryThings.first()->setStateValue(batteryBatteryLevelStateTypeId, batterySoc);
                batteryThings.first()->setStateValue(batteryBatteryCriticalStateTypeId, client->batteryPercentage() <= 10);

                float power = client->batteryPower();
                if (power < 1 && power > -1) {
                    power = 0;
                }
                batteryThings.first()->setStateValue(batteryCurrentPowerStateTypeId, power);
                if (power > 1) {
                    batteryThings.first()->setStateValue(batteryChargingStateStateTypeId, "charging");
                } else if (power < -1) {
                    batteryThings.first()->setStateValue(batteryChargingStateStateTypeId, "discharging");
                } else {
                    batteryThings.first()->setStateValue(batteryChargingStateStateTypeId, "idle");
                }
            }
        }
    });

    m_clients.insert(thing, client);
    client->connectToDevice();

    info->finish(Thing::ThingErrorNoError);
}
