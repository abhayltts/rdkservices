/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include "SystemServices.h"

#include "FactoriesImplementation.h"
#include "HostMock.h"
#include "IarmBusMock.h"
#include "RfcApiMock.h"
#include "ServiceMock.h"
#include "SleepModeMock.h"
#include "WrapsMock.h"

#include "deepSleepMgr.h"
#include "exception.hpp"

#include <fstream>

using namespace WPEFramework;

class SystemServicesTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::SystemServices> plugin;
    Core::JSONRPC::Handler& handler;
    Core::JSONRPC::Connection connection;
    string response;
    RfcApiImplMock rfcApiImplMock;
    WrapsImplMock wrapsImplMock;
    IarmBusImplMock iarmBusImplMock;
    HostImplMock hostImplMock;

    SystemServicesTest()
        : plugin(Core::ProxyType<Plugin::SystemServices>::Create())
        , handler(*plugin)
        , connection(1, 0)
    {
        RfcApi::getInstance().impl = &rfcApiImplMock;
        Wraps::getInstance().impl = &wrapsImplMock;
        IarmBus::getInstance().impl = &iarmBusImplMock;
        device::Host::getInstance().impl = &hostImplMock;
    }

    virtual ~SystemServicesTest() override
    {
        RfcApi::getInstance().impl = nullptr;
        Wraps::getInstance().impl = nullptr;
        IarmBus::getInstance().impl = nullptr;
        device::Host::getInstance().impl = nullptr;
    }
};

class SystemServicesEventTest : public SystemServicesTest {
protected:
    ServiceMock service;
    Core::JSONRPC::Message message;
    FactoriesImplementation factoriesImplementation;
    PluginHost::IDispatcher* dispatcher;

    SystemServicesEventTest()
        : SystemServicesTest()
    {
        PluginHost::IFactories::Assign(&factoriesImplementation);

        dispatcher = static_cast<PluginHost::IDispatcher*>(
            plugin->QueryInterface(PluginHost::IDispatcher::ID));
        dispatcher->Activate(&service);
    }

    virtual ~SystemServicesEventTest() override
    {
        dispatcher->Deactivate();
        dispatcher->Release();

        PluginHost::IFactories::Assign(nullptr);
    }
};

TEST_F(SystemServicesTest, RegisterMethods)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("requestSystemUptime")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("fireFirmwarePendingReboot")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setFirmwareAutoReboot")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setFirmwareRebootDelay")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getLastFirmwareFailureReason")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getDownloadedFirmwareInfo")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getFirmwareDownloadPercent")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getFirmwareUpdateState")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setTimeZoneDST")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getTimeZoneDST")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setTerritory")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getTerritory")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getPreviousRebootInfo")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getPreviousRebootInfo2")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getPreviousRebootReason")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("isOptOutTelemetry")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setOptOutTelemetry")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getSystemVersions")));
}

TEST_F(SystemServicesTest, SystemUptime)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("requestSystemUptime"), _T("{}"), response));

    EXPECT_THAT(response, ::testing::MatchesRegex(_T("\\{"
                                                     "\"systemUptime\":\"[0-9]+.[0-9]+\","
                                                     "\"success\":true"
                                                     "\\}")));
}

TEST_F(SystemServicesEventTest, PendingReboot)
{
    Core::Event onFirmwarePendingReboot(false, true);

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));
                EXPECT_EQ(text, string(_T("{"
                                          "\"jsonrpc\":\"2.0\","
                                          "\"method\":\"org.rdk.System.onFirmwarePendingReboot\","
                                          "\"params\":"
                                          "{"
                                          "\"fireFirmwarePendingReboot\":600"
                                          "}"
                                          "}")));

                onFirmwarePendingReboot.SetEvent();

                return Core::ERROR_NONE;
            }));

    handler.Subscribe(0, _T("onFirmwarePendingReboot"), _T("org.rdk.System"), message);

    EXPECT_CALL(rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                EXPECT_EQ(strcmp(pcCallerID, "thunderapi"), 0);
                EXPECT_EQ(strcmp(pcParameterName, "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.AutoReboot.fwDelayReboot"), 0);
                return WDMP_SUCCESS;
            }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("fireFirmwarePendingReboot"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, onFirmwarePendingReboot.Lock());

    handler.Unsubscribe(0, _T("onFirmwarePendingReboot"), _T("org.rdk.System"), message);
}

TEST_F(SystemServicesTest, AutoReboot)
{
    EXPECT_CALL(rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                EXPECT_EQ(strcmp(pcCallerID, "thunderapi"), 0);
                EXPECT_EQ(strcmp(pcParameterName, "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.AutoReboot.Enable"), 0);
                return WDMP_SUCCESS;
            }));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setFirmwareAutoReboot"), _T("{}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setFirmwareAutoReboot"), _T("{\"enable\":true}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));
}

TEST_F(SystemServicesTest, RebootDelay)
{
    EXPECT_CALL(rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                EXPECT_EQ(strcmp(pcCallerID, "thunderapi"), 0);
                EXPECT_EQ(strcmp(pcParameterName, "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.AutoReboot.fwDelayReboot"), 0);
                return WDMP_SUCCESS;
            }));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setFirmwareRebootDelay"), _T("{}"), response));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setFirmwareRebootDelay"), _T("{\"delaySeconds\":86401}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setFirmwareRebootDelay"), _T("{\"delaySeconds\":10}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));
}

TEST_F(SystemServicesTest, Firmware)
{
    ofstream file("/version.txt");
    file << "imagename:PX051AEI_VBN_2203_sprint_20220331225312sdy_NG\nSDK_VERSION=17.3\nMEDIARITE=8.3.53\nYOCTO_VERSION=dunfell\nVERSION=000.36.0.0\nBUILD_TIME=\"2022-08-05 16:14:54\"\n";
    file.close();

    file.open("/opt/fwdnldstatus.txt");
    file << "Method|xconf \nProto|\nStatus|Failure\nReboot|\nFailureReason|Invalid Request\nDnldVersn|AX013AN_5.6p4s1_VBN_sey\nDnldFile|\nDnlURL|https://dac15cdlserver.ae.ccp.xcal.tv:443/Images/AX013AN_5.6p4s1_VBN_sey-signed.bin\n FwupdateState|Failed\n";

    file.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getLastFirmwareFailureReason"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"failReason\":\"None\",\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDownloadedFirmwareInfo"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"currentFWVersion\":\"PX051AEI_VBN_2203_sprint_20220331225312sdy_NG\",\"downloadedFWVersion\":\"\",\"downloadedFWLocation\":\"\",\"isRebootDeferred\":false,\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getFirmwareDownloadPercent"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"downloadPercent\":-1,\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getFirmwareUpdateState"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"firmwareUpdateState\":0,\"success\":true}"));
}

TEST_F(SystemServicesEventTest, Timezone)
{
    Core::Event changed1(false, true);
    Core::Event changed2(false, true);

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(2)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));
                EXPECT_THAT(text, ::testing::MatchesRegex(_T("\\{"
                                                             "\"jsonrpc\":\"2.0\","
                                                             "\"method\":\"org.rdk.System.onTimeZoneDSTChanged\","
                                                             "\"params\":"
                                                             "\\{"
                                                             "\"oldTimeZone\":\".*\","
                                                             "\"newTimeZone\":\"America\\\\/New_York\""
                                                             "\\}"
                                                             "\\}")));

                changed1.SetEvent();

                return Core::ERROR_NONE;
            }))
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));
                EXPECT_EQ(text, string(_T("{"
                                          "\"jsonrpc\":\"2.0\","
                                          "\"method\":\"org.rdk.System.onTimeZoneDSTChanged\","
                                          "\"params\":"
                                          "{"
                                          "\"oldTimeZone\":\"America\\/New_York\","
                                          "\"newTimeZone\":\"America\\/Costa_Rica\""
                                          "}"
                                          "}")));

                changed2.SetEvent();

                return Core::ERROR_NONE;
            }));

    handler.Subscribe(0, _T("onTimeZoneDSTChanged"), _T("org.rdk.System"), message);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setTimeZoneDST"), _T("{\"timeZone\":\"America/New_York\"}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, changed1.Lock());

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getTimeZoneDST"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"timeZone\":\"America\\/New_York\",\"success\":true}"));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setTimeZoneDST"), _T("{\"timeZone\":\"America/Costa_Rica\"}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, changed2.Lock());

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getTimeZoneDST"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"timeZone\":\"America\\/Costa_Rica\",\"success\":true}"));

    handler.Unsubscribe(0, _T("onTimeZoneDSTChanged"), _T("org.rdk.System"), message);
}

TEST_F(SystemServicesTest, InvalidTerritory)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setTerritory"), _T("{\"territory\":\"USA\",\"region\":\"U-NYC\"}"), response));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setTerritory"), _T("{\"territory\":\"US@\",\"region\":\"US-NYC\"}"), response));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setTerritory"), _T("{\"territory\":\"USA\",\"region\":\"US-N$C\"}"), response));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setTerritory"), _T("{\"territory\":\"US12\",\"region\":\"US-NYC\"}"), response));
}

TEST_F(SystemServicesEventTest, ValidTerritory)
{
    Core::Event changed1(false, true);
    Core::Event changed2(false, true);

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(2)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));
                EXPECT_THAT(text, ::testing::MatchesRegex(_T("\\{"
                                                             "\"jsonrpc\":\"2.0\","
                                                             "\"method\":\"org.rdk.System.onTerritoryChanged\","
                                                             "\"params\":"
                                                             "\\{"
                                                             "\"oldTerritory\":\".*\","
                                                             "\"newTerritory\":\"USA\","
                                                             "\"oldRegion\":\".*\","
                                                             "\"newRegion\":\"US-NYC\""
                                                             "\\}"
                                                             "\\}")));

                changed1.SetEvent();

                return Core::ERROR_NONE;
            }))
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));
                EXPECT_EQ(text, string(_T("{"
                                          "\"jsonrpc\":\"2.0\","
                                          "\"method\":\"org.rdk.System.onTerritoryChanged\","
                                          "\"params\":"
                                          "{"
                                          "\"oldTerritory\":\"USA\","
                                          "\"newTerritory\":\"GBR\","
                                          "\"oldRegion\":\"US-NYC\","
                                          "\"newRegion\":\"GB-EGL\""
                                          "}"
                                          "}")));

                changed2.SetEvent();

                return Core::ERROR_NONE;
            }));

    handler.Subscribe(0, _T("onTerritoryChanged"), _T("org.rdk.System"), message);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setTerritory"), _T("{\"territory\":\"USA\",\"region\":\"US-NYC\"}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, changed1.Lock());

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getTerritory"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"territory\":\"USA\",\"region\":\"US-NYC\",\"success\":true}"));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setTerritory"), _T("{\"territory\":\"GBR\",\"region\":\"GB-EGL\"}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, changed2.Lock());

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getTerritory"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"territory\":\"GBR\",\"region\":\"GB-EGL\",\"success\":true}"));

    handler.Unsubscribe(0, _T("onTerritoryChanged"), _T("org.rdk.System"), message);
}

TEST_F(SystemServicesTest, rebootReason)
{
    ofstream file("/opt/logs/rebootInfo.log");
    file << "PreviousRebootReason: RebootReason: Triggered from SystemServices! MAINTENANCE_REBOOT \n PreviousRebootTime: 18.08.2022_09:51.38\nPreviousRebootInitiatedBy: HAL_CDL_notify_mgr_event\nPreviousCustomReason: MAINTENANCE_REBOOT \n PreviousOtherReason: MAINTENANCE_REBOOT \n";
    file.close();

    file.open("/opt/persistent/previousreboot.info");
    file << "{\n\"timestamp\":\"Thu Aug 18 13:51:39 UTC 2022\",\n \"source\":\"HAL_CDL_notify_mgr_event\",\n \"reason\":\"OPS_TRIGGERED\",\n \"customReason\":\"Unknown\",\n \"otherReason\":\"Rebooting the box due to VL_CDL_MANAGER_EVENT_REBOOT...!\"\n}\n";
    file.close();

    file.open("/opt/persistent/hardpower.info");
    file << "{\n\"lastHardPowerReset\":\"Thu Aug 18 13:51:39 UTC 2022\"\n}\n";
    file.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPreviousRebootInfo"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"timeStamp\":\"18.08.2022_09:51.38\",\"reason\":\"Triggered from SystemServices! MAINTENANCE_REBOOT\",\"source\":\"HAL_CDL_notify_mgr_event\",\"customReason\":\"MAINTENANCE_REBOOT\",\"otherReason\":\"MAINTENANCE_REBOOT\",\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPreviousRebootInfo2"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"rebootInfo\":{\"timestamp\":\"Thu Aug 18 13:51:39 UTC 2022\",\"source\":\"HAL_CDL_notify_mgr_event\",\"reason\":\"OPS_TRIGGERED\",\"customReason\":\"Unknown\",\"lastHardPowerReset\":\"Thu Aug 18 13:51:39 UTC 2022\"},\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPreviousRebootReason"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"reason\":\"OPS_TRIGGERED\",\"success\":true}"));
}

TEST_F(SystemServicesTest, Telemetry)
{
    ofstream file("/opt/tmtryoptout");
    file << "false";
    file.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("isOptOutTelemetry"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"Opt-Out\":false,\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setOptOutTelemetry"), _T("{\"Opt-Out\":true}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("isOptOutTelemetry"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"Opt-Out\":true,\"success\":true}"));
}

TEST_F(SystemServicesTest, SystemVersions)
{
    ofstream file("/version.txt");
    file << "imagename:PX051AEI_VBN_2203_sprint_20220331225312sdy_NG\nSDK_VERSION=17.3\nMEDIARITE=8.3.53\nYOCTO_VERSION=dunfell\nVERSION=000.36.0.0\nBUILD_TIME=\"2022-08-05 16:14:54\"\n";
    file.close();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getSystemVersions"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"stbVersion\":\"PX051AEI_VBN_2203_sprint_20220331225312sdy_NG\",\"receiverVersion\":\"000.36.0.0\",\"stbTimestamp\":\"Fri 05 Aug 2022 16:14:54 AP UTC\",\"success\":true}"));
}

TEST_F(SystemServicesTest, MocaStatus)
{
    ON_CALL(wrapsImplMock, system(::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](const char* command) {
                EXPECT_EQ(string(command), string(_T("/etc/init.d/moca_init start")));
                return 0;
            }));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("enableMoca"), _T("{}"), response));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("enableMoca"), _T("{\"value\":true}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));
    EXPECT_TRUE(Core::File(string(_T("/opt/enablemoca"))).Exists());
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("queryMocaStatus"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"mocaEnabled\":true,\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("enableMoca"), _T("{\"value\":false}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));
    EXPECT_FALSE(Core::File(string(_T("/opt/enablemoca"))).Exists());
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("queryMocaStatus"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"mocaEnabled\":false,\"success\":true}"));
}

TEST_F(SystemServicesTest, updateFirmware)
{
    ON_CALL(wrapsImplMock, popen(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](const char* command, const char* type) {
                EXPECT_EQ(string(command), string(_T("/lib/rdk/deviceInitiatedFWDnld.sh 0 4 >> /opt/logs/swupdate.log &")));
                return nullptr;
            }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("updateFirmware"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));
}

TEST_F(SystemServicesTest, Mode)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getMode"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"modeInfo\":{\"mode\":\"\",\"duration\":0},\"success\":true}"));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setMode"), _T("{}"), response));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setMode"), _T("{\"modeInfo\":{}}"), response));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setMode"), _T("{\"modeInfo\":{\"mode\":\"unknown\",\"duration\":0}}"), response));

    ON_CALL(iarmBusImplMock, IARM_Bus_Call)
        .WillByDefault(
            [](const char* ownerName, const char* methodName, void* arg, size_t argLen) {
                EXPECT_EQ(string(ownerName), string(_T(IARM_BUS_DAEMON_NAME)));
                EXPECT_EQ(string(methodName), string(_T("DaemonSysModeChange")));
                return IARM_RESULT_SUCCESS;
            });

    ON_CALL(wrapsImplMock, system(::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](const char* command) {
                EXPECT_EQ(string(command), string(_T("rm -f /opt/warehouse_mode_active")));
                return 0;
            }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setMode"), _T("{\"modeInfo\":{\"mode\":\"NORMAL\",\"duration\":-1}}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getMode"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"modeInfo\":{\"mode\":\"NORMAL\",\"duration\":0},\"success\":true}"));
}

TEST_F(SystemServicesTest, setDeepSleepTimer)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setDeepSleepTimer"), _T("{}"), response));

    ON_CALL(iarmBusImplMock, IARM_Bus_Call)
        .WillByDefault(
            [](const char* ownerName, const char* methodName, void* arg, size_t argLen) {
                EXPECT_EQ(string(ownerName), string(_T(IARM_BUS_PWRMGR_NAME)));
                EXPECT_EQ(string(methodName), string(_T(IARM_BUS_PWRMGR_API_SetDeepSleepTimeOut)));
                return IARM_RESULT_SUCCESS;
            });

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setDeepSleepTimer"), _T("{\"seconds\":5}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));
}

TEST_F(SystemServicesTest, setNetworkStandbyMode)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setNetworkStandbyMode"), _T("{}"), response));

    ON_CALL(iarmBusImplMock, IARM_Bus_Call)
        .WillByDefault(
            [](const char* ownerName, const char* methodName, void* arg, size_t argLen) {
                EXPECT_EQ(string(ownerName), string(_T(IARM_BUS_PWRMGR_NAME)));
                EXPECT_EQ(string(methodName), string(_T(IARM_BUS_PWRMGR_API_SetNetworkStandbyMode)));
                return IARM_RESULT_SUCCESS;
            });

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setNetworkStandbyMode"), _T("{\"nwStandby\":true}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));
}

TEST_F(SystemServicesTest, getNetworkStandbyMode)
{
    ON_CALL(iarmBusImplMock, IARM_Bus_Call)
        .WillByDefault(
            [](const char* ownerName, const char* methodName, void* arg, size_t argLen) {
                EXPECT_EQ(string(ownerName), string(_T(IARM_BUS_PWRMGR_NAME)));
                EXPECT_EQ(string(methodName), string(_T(IARM_BUS_PWRMGR_API_GetNetworkStandbyMode)));
                auto param = static_cast<IARM_Bus_PWRMgr_NetworkStandbyMode_Param_t*>(arg);
                param->bStandbyMode = true;
                return IARM_RESULT_SUCCESS;
            });

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getNetworkStandbyMode"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"nwStandby\":true,\"success\":true}"));
}

TEST_F(SystemServicesTest, setPreferredStandbyMode)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setPreferredStandbyMode"), _T("{}"), response));

    ON_CALL(hostImplMock, setPreferredSleepMode)
        .WillByDefault(::testing::Return(0));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPreferredStandbyMode"), _T("{\"standbyMode\":\"LIGHT_SLEEP\"}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));

    ON_CALL(hostImplMock, setPreferredSleepMode)
        .WillByDefault(::testing::Invoke(
            [](const device::SleepMode) -> int {
                throw device::Exception("test");
            }));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setPreferredStandbyMode"), _T("{\"standbyMode\":\"LIGHT_SLEEP\"}"), response));
}

TEST_F(SystemServicesTest, getPreferredStandbyMode)
{
    device::SleepMode mode;
    SleepModeMock sleepModeMock;
    mode.impl = &sleepModeMock;
    string sleepModeString(_T("DEEP_SLEEP"));

    ON_CALL(hostImplMock, getPreferredSleepMode)
        .WillByDefault(::testing::Return(mode));
    ON_CALL(sleepModeMock, toString)
        .WillByDefault(::testing::ReturnRef(sleepModeString));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPreferredStandbyMode"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"preferredStandbyMode\":\"DEEP_SLEEP\",\"success\":true}"));

    ON_CALL(hostImplMock, getPreferredSleepMode)
        .WillByDefault(::testing::Invoke(
            []() -> device::SleepMode {
                throw device::Exception("test");
            }));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getPreferredStandbyMode"), _T("{}"), response));
}

TEST_F(SystemServicesTest, getAvailableStandbyModes)
{
    device::SleepMode mode;
    SleepModeMock sleepModeMock;
    mode.impl = &sleepModeMock;
    string sleepModeString(_T("DEEP_SLEEP"));

    ON_CALL(hostImplMock, getAvailableSleepModes)
        .WillByDefault(::testing::Return(std::vector<device::SleepMode>({ mode })));
    ON_CALL(sleepModeMock, toString)
        .WillByDefault(::testing::ReturnRef(sleepModeString));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getAvailableStandbyModes"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"supportedStandbyModes\":[\"DEEP_SLEEP\"],\"success\":true}"));

    ON_CALL(hostImplMock, getAvailableSleepModes)
        .WillByDefault(::testing::Invoke(
            []() -> device::List<device::SleepMode> {
                throw device::Exception("test");
            }));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getAvailableStandbyModes"), _T("{}"), response));
}

TEST_F(SystemServicesTest, getWakeupReason)
{
    ON_CALL(iarmBusImplMock, IARM_Bus_Call)
        .WillByDefault(
            [](const char* ownerName, const char* methodName, void* arg, size_t argLen) {
                EXPECT_EQ(string(ownerName), string(_T(IARM_BUS_DEEPSLEEPMGR_NAME)));
                EXPECT_EQ(string(methodName), string(_T(IARM_BUS_DEEPSLEEPMGR_API_GetLastWakeupReason)));
                auto param = static_cast<DeepSleep_WakeupReason_t*>(arg);
                *param = DEEPSLEEP_WAKEUPREASON_IR;
                return IARM_RESULT_SUCCESS;
            });

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getWakeupReason"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"wakeupReason\":\"WAKEUP_REASON_IR\",\"success\":true}"));
}

TEST_F(SystemServicesTest, getLastWakeupKeyCode)
{
    ON_CALL(iarmBusImplMock, IARM_Bus_Call)
        .WillByDefault(
            [](const char* ownerName, const char* methodName, void* arg, size_t argLen) {
                EXPECT_EQ(string(ownerName), string(_T(IARM_BUS_DEEPSLEEPMGR_NAME)));
                EXPECT_EQ(string(methodName), string(_T(IARM_BUS_DEEPSLEEPMGR_API_GetLastWakeupKeyCode)));
                auto param = static_cast<IARM_Bus_DeepSleepMgr_WakeupKeyCode_Param_t*>(arg);
                param->keyCode = 5;
                return IARM_RESULT_SUCCESS;
            });

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getLastWakeupKeyCode"), _T("{}"), response));
    EXPECT_EQ(response, string("{\"wakeupKeyCode\":5,\"success\":true}"));
}
