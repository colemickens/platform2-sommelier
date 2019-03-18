// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/newblue_daemon.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include <base/memory/ref_counted.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/mock_object_manager.h>
#include <dbus/mock_object_proxy.h>
#include <dbus/object_manager.h>
#include <gtest/gtest.h>

#include "bluetooth/common/util.h"
#include "bluetooth/newblued/mock_libnewblue.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;

namespace bluetooth {

namespace {

constexpr char kTestSender[] = ":1.1";
constexpr char kTestSender2[] = ":1.2";
constexpr int kTestSerial = 10;
constexpr char kTestDeviceAddress[] = "06:05:04:03:02:01";
constexpr char kTestDeviceObjectPath[] =
    "/org/bluez/hci0/dev_06_05_04_03_02_01";

constexpr uniq_t kTestDiscoveryId = 7;

void SaveResponse(std::unique_ptr<dbus::Response>* saved_response,
                  std::unique_ptr<dbus::Response> response) {
  *saved_response = std::move(response);
}

}  // namespace

class NewblueDaemonTest : public ::testing::Test {
 public:
  using MethodHandlerMap =
      std::map<std::string, dbus::ExportedObject::MethodCallCallback*>;

  void SetUp() override {
    bus_ = new dbus::MockBus(dbus::Bus::Options());
    EXPECT_CALL(*bus_, GetDBusTaskRunner())
        .WillRepeatedly(Return(message_loop_.task_runner().get()));
    EXPECT_CALL(*bus_, AssertOnOriginThread()).Times(AnyNumber());
    auto libnewblue = std::make_unique<MockLibNewblue>();
    libnewblue_ = libnewblue.get();
    auto newblue = std::make_unique<Newblue>(std::move(libnewblue));
    newblue_daemon_ = std::make_unique<NewblueDaemon>(std::move(newblue),
                                                      false /* is_idle_mode */);
    SetupBluezObjectProxy();
    SetupBluezObjectManager();
    // Force MessageLoop to run all pending tasks as an effect of instantiating
    // MockObjectManager. This is needed to avoid memory leak as pending tasks
    // hold pointers.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  dbus::ExportedObject::MethodCallCallback* GetMethodHandler(
      MethodHandlerMap method_handlers, const std::string& method_name) {
    return base::ContainsKey(method_handlers, method_name)
               ? method_handlers[method_name]
               : &dummy_method_handler_;
  }

  // Expects that the standard methods on org.freedesktop.DBus.Properties
  // interface are exported.
  void ExpectPropertiesMethodsExportedAsync(
      scoped_refptr<dbus::MockExportedObject> exported_object) {
    EXPECT_CALL(*exported_object, ExportMethod(dbus::kPropertiesInterface,
                                               dbus::kPropertiesGet, _, _))
        .Times(1);
    EXPECT_CALL(*exported_object, ExportMethod(dbus::kPropertiesInterface,
                                               dbus::kPropertiesSet, _, _))
        .Times(1);
    EXPECT_CALL(*exported_object, ExportMethod(dbus::kPropertiesInterface,
                                               dbus::kPropertiesGetAll, _, _))
        .Times(1);
  }

  // Expects that the standard methods on org.freedesktop.DBus.Properties
  // interface are exported.
  void ExpectPropertiesMethodsExportedSync(
      scoped_refptr<dbus::MockExportedObject> exported_object) {
    EXPECT_CALL(*exported_object,
                ExportMethodAndBlock(dbus::kPropertiesInterface,
                                     dbus::kPropertiesGet, _))
        .WillOnce(Return(true));
    EXPECT_CALL(*exported_object,
                ExportMethodAndBlock(dbus::kPropertiesInterface,
                                     dbus::kPropertiesSet, _))
        .WillOnce(Return(true));
    EXPECT_CALL(*exported_object,
                ExportMethodAndBlock(dbus::kPropertiesInterface,
                                     dbus::kPropertiesGetAll, _))
        .WillOnce(Return(true));
  }

  // Expects that the methods on org.bluez.Device1 interface are exported.
  void ExpectDeviceMethodsExported(
      scoped_refptr<dbus::MockExportedObject> exported_object) {
    EXPECT_CALL(
        *exported_object,
        ExportMethodAndBlock(bluetooth_device::kBluetoothDeviceInterface,
                             bluetooth_device::kPair, _))
        .WillOnce(Return(true));
    EXPECT_CALL(
        *exported_object,
        ExportMethodAndBlock(bluetooth_device::kBluetoothDeviceInterface,
                             bluetooth_device::kCancelPairing, _))
        .WillOnce(Return(true));
    EXPECT_CALL(
        *exported_object,
        ExportMethodAndBlock(bluetooth_device::kBluetoothDeviceInterface,
                             bluetooth_device::kConnect, _))
        .WillOnce(Return(true));
  }

  // Expects that the methods on org.bluez.Device1 interface are unexported.
  void ExpectDeviceMethodsUnexported(
      scoped_refptr<dbus::MockExportedObject> exported_object) {
    EXPECT_CALL(
        *exported_object,
        UnexportMethodAndBlock(bluetooth_device::kBluetoothDeviceInterface,
                               bluetooth_device::kPair))
        .WillOnce(Return(true));
    EXPECT_CALL(
        *exported_object,
        UnexportMethodAndBlock(bluetooth_device::kBluetoothDeviceInterface,
                               bluetooth_device::kCancelPairing))
        .WillOnce(Return(true));
    EXPECT_CALL(
        *exported_object,
        UnexportMethodAndBlock(bluetooth_device::kBluetoothDeviceInterface,
                               bluetooth_device::kConnect))
        .WillOnce(Return(true));
  }

  // Expects that the methods on org.bluez.AdvertisingManager1 interface are
  // exported.
  void ExpectAdvertisingManagerMethodsExported(
      scoped_refptr<dbus::MockExportedObject> exported_object) {
    EXPECT_CALL(*exported_object,
                ExportMethodAndBlock(
                    bluetooth_advertising_manager::
                        kBluetoothAdvertisingManagerInterface,
                    bluetooth_advertising_manager::kRegisterAdvertisement, _))
        .WillOnce(Return(true));
    EXPECT_CALL(*exported_object,
                ExportMethodAndBlock(
                    bluetooth_advertising_manager::
                        kBluetoothAdvertisingManagerInterface,
                    bluetooth_advertising_manager::kUnregisterAdvertisement, _))
        .WillOnce(Return(true));
  }

  // Expects that the methods on org.bluez.AgentManager1 interface are
  // exported.
  void ExpectAgentManagerMethodsExported(
      scoped_refptr<dbus::MockExportedObject> exported_object) {
    EXPECT_CALL(*exported_object,
                ExportMethodAndBlock(
                    bluetooth_agent_manager::kBluetoothAgentManagerInterface,
                    bluetooth_agent_manager::kRegisterAgent, _))
        .WillOnce(Return(true));
    EXPECT_CALL(*exported_object,
                ExportMethodAndBlock(
                    bluetooth_agent_manager::kBluetoothAgentManagerInterface,
                    bluetooth_agent_manager::kUnregisterAgent, _))
        .WillOnce(Return(true));
    EXPECT_CALL(*exported_object,
                ExportMethodAndBlock(
                    bluetooth_agent_manager::kBluetoothAgentManagerInterface,
                    bluetooth_agent_manager::kRequestDefaultAgent, _))
        .WillOnce(Return(true));
  }

  scoped_refptr<dbus::MockExportedObject> AddOrGetMockExportedObject(
      const dbus::ObjectPath& object_path) {
    if (base::ContainsKey(mock_exported_objects_, object_path))
      return mock_exported_objects_[object_path];

    scoped_refptr<dbus::MockExportedObject> exported_object =
        new dbus::MockExportedObject(bus_.get(), object_path);
    mock_exported_objects_[object_path] = exported_object;
    return exported_object;
  }

  void ExpectDeviceObjectExported(const dbus::ObjectPath& device_object_path) {
    scoped_refptr<dbus::MockExportedObject> exported_dev_object =
        AddOrGetMockExportedObject(device_object_path);
    ExpectDeviceMethodsExported(exported_dev_object);
    ExpectPropertiesMethodsExportedSync(exported_dev_object);
    EXPECT_CALL(*bus_, GetExportedObject(device_object_path))
        .WillOnce(Return(exported_dev_object.get()));
  }

  void ExpectDeviceObjectUnexported(
      const dbus::ObjectPath& device_object_path) {
    scoped_refptr<dbus::MockExportedObject> exported_dev_object =
        AddOrGetMockExportedObject(device_object_path);
    ExpectDeviceMethodsUnexported(exported_dev_object);
  }

  scoped_refptr<dbus::MockExportedObject> SetupExportedRootObject() {
    dbus::ObjectPath root_path(
        newblue_object_manager::kNewblueObjectManagerServicePath);
    scoped_refptr<dbus::MockExportedObject> exported_root_object =
        new dbus::MockExportedObject(bus_.get(), root_path);
    EXPECT_CALL(*bus_, GetExportedObject(root_path))
        .WillRepeatedly(Return(exported_root_object.get()));
    return exported_root_object;
  }

  scoped_refptr<dbus::MockExportedObject> SetupExportedAgentManagerObject() {
    dbus::ObjectPath agent_manager_path(
        bluetooth_agent_manager::kBluetoothAgentManagerServicePath);
    scoped_refptr<dbus::MockExportedObject> exported_agent_manager_object =
        new dbus::MockExportedObject(bus_.get(), agent_manager_path);
    EXPECT_CALL(*bus_, GetExportedObject(agent_manager_path))
        .WillRepeatedly(Return(exported_agent_manager_object.get()));
    return exported_agent_manager_object;
  }

  scoped_refptr<dbus::MockExportedObject> SetupExportedAdapterObject() {
    dbus::ObjectPath adapter_object_path(kAdapterObjectPath);
    scoped_refptr<dbus::MockExportedObject> exported_adapter_object =
        new dbus::MockExportedObject(bus_.get(), adapter_object_path);
    EXPECT_CALL(*bus_, GetExportedObject(adapter_object_path))
        .WillRepeatedly(Return(exported_adapter_object.get()));
    return exported_adapter_object;
  }

  void SetupBluezObjectProxy() {
    dbus::ObjectPath object_path(
        bluez_object_manager::kBluezObjectManagerServicePath);
    bluez_object_proxy_ = new dbus::MockObjectProxy(
        bus_.get(), bluez_object_manager::kBluezObjectManagerServiceName,
        object_path);
    EXPECT_CALL(*bus_, GetObjectProxy(
                           bluez_object_manager::kBluezObjectManagerServiceName,
                           object_path))
        .WillRepeatedly(Return(bluez_object_proxy_.get()));
  }

  void SetupBluezObjectManager() {
    dbus::ObjectPath object_path(
        bluez_object_manager::kBluezObjectManagerServicePath);
    bluez_object_manager_ = new dbus::MockObjectManager(
        bus_.get(), bluez_object_manager::kBluezObjectManagerServiceName,
        object_path);
    EXPECT_CALL(*bus_, GetObjectManager(
                           bluez_object_manager::kBluezObjectManagerServiceName,
                           object_path))
        .WillRepeatedly(Return(bluez_object_manager_.get()));
  }

  struct smKnownDevNode* CreateSmKnownDeviceNode(const std::string& address,
                                                 bool is_random_address,
                                                 bool is_paired,
                                                 std::string name) {
    struct smKnownDevNode* node =
        (struct smKnownDevNode*)calloc(1, sizeof(struct smKnownDevNode));
    ConvertToBtAddr(is_random_address, address, &node->addr);
    node->isPaired = is_paired;
    node->name = strdup(name.c_str());
    return node;
  }

  struct smKnownDevNode* StubGetKnownDevices() {
    struct smKnownDevNode* node1 = CreateSmKnownDeviceNode(
        "01:AA:BB:CC:DD:EE", /* is_random_address */ true,
        /* is_paired */ true, "Test Device 1");
    struct smKnownDevNode* node2 = CreateSmKnownDeviceNode(
        "02:AA:BB:CC:DD:EE", /* is_random_address */ true,
        /* is_paired */ false, "Test Device 2");
    struct smKnownDevNode* node3 = CreateSmKnownDeviceNode(
        "03:AA:BB:CC:DD:EE", /* is_random_address */ false,
        /* is_paired */ true, "Test Device 3");
    node1->next = node2;
    node2->next = node3;
    node3->next = nullptr;
    return node1;
  }

  void ExpectTestInit(
      scoped_refptr<dbus::MockExportedObject> exported_root_object) {
    EXPECT_CALL(*bus_,
                RequestOwnershipAndBlock(
                    newblue_object_manager::kNewblueObjectManagerServiceName,
                    dbus::Bus::REQUIRE_PRIMARY))
        .WillOnce(Return(true));

    // Standard methods on org.freedesktop.DBus.ObjectManager interface should
    // be exported.
    EXPECT_CALL(*exported_root_object,
                ExportMethod(dbus::kObjectManagerInterface,
                             dbus::kObjectManagerGetManagedObjects, _, _))
        .Times(1);
    // Standard methods on org.freedesktop.DBus.Properties interface should be
    // exported.
    ExpectPropertiesMethodsExportedAsync(exported_root_object);
  }

  void TestInit(scoped_refptr<dbus::MockExportedObject> exported_root_object) {
    ExpectTestInit(exported_root_object);

    EXPECT_CALL(*libnewblue_, HciUp(_, _, _)).WillOnce(Return(true));
    EXPECT_TRUE(newblue_daemon_->Init(
        bus_, nullptr /* no need to access the delegator */));
  }

  void TestAdapterBringUp(
      scoped_refptr<dbus::MockExportedObject> exported_adapter_object,
      MethodHandlerMap adapter_method_handlers) {
    // org.bluez.Adapter1 methods
    EXPECT_CALL(
        *exported_adapter_object,
        ExportMethodAndBlock(bluetooth_adapter::kBluetoothAdapterInterface,
                             bluetooth_adapter::kStartDiscovery, _))
        .WillOnce(DoAll(
            SaveArg<2>(GetMethodHandler(adapter_method_handlers,
                                        bluetooth_adapter::kStartDiscovery)),
            Return(true)));

    EXPECT_CALL(
        *exported_adapter_object,
        ExportMethodAndBlock(bluetooth_adapter::kBluetoothAdapterInterface,
                             bluetooth_adapter::kStopDiscovery, _))
        .WillOnce(DoAll(
            SaveArg<2>(GetMethodHandler(adapter_method_handlers,
                                        bluetooth_adapter::kStopDiscovery)),
            Return(true)));

    EXPECT_CALL(
        *exported_adapter_object,
        ExportMethodAndBlock(bluetooth_adapter::kBluetoothAdapterInterface,
                             bluetooth_adapter::kRemoveDevice, _))
        .WillOnce(DoAll(
            SaveArg<2>(GetMethodHandler(adapter_method_handlers,
                                        bluetooth_adapter::kRemoveDevice)),
            Return(true)));

    EXPECT_CALL(*libnewblue_, HciIsUp()).WillOnce(Return(true));
    EXPECT_CALL(*libnewblue_, L2cInit()).WillOnce(Return(0));
    EXPECT_CALL(*libnewblue_, AttInit()).WillOnce(Return(true));
    EXPECT_CALL(*libnewblue_, GattProfileInit()).WillOnce(Return(true));
    EXPECT_CALL(*libnewblue_, GattBuiltinInit()).WillOnce(Return(true));
    EXPECT_CALL(*libnewblue_, SmInit()).WillOnce(Return(true));
    EXPECT_CALL(*libnewblue_, SmRegisterPasskeyDisplayObserver(_, _))
        .WillOnce(Return(true));
    EXPECT_CALL(*libnewblue_, SmRegisterPairStateObserver(_, _))
        .WillOnce(Return(true));

    // At initialization, newblued should export the saved paired devices.
    ExpectDeviceObjectExported(
        dbus::ObjectPath("/org/bluez/hci0/dev_01_AA_BB_CC_DD_EE"));
    ExpectDeviceObjectExported(
        dbus::ObjectPath("/org/bluez/hci0/dev_03_AA_BB_CC_DD_EE"));
    struct smKnownDevNode* known_devices = StubGetKnownDevices();
    EXPECT_CALL(*libnewblue_, SmGetKnownDevices())
        .WillOnce(Return(known_devices));
    EXPECT_CALL(*libnewblue_, SmKnownDevicesFree(known_devices))
        .WillOnce(Invoke(&smKnownDevicesFree));

    newblue_daemon_->OnHciReadyForUp();
  }

  void ConstructRemoveDeviceMethodCall(
      dbus::MethodCall* remove_device_method_call,
      const std::string device_object_path) {
    remove_device_method_call->SetPath(dbus::ObjectPath(kAdapterObjectPath));
    remove_device_method_call->SetSender(kTestSender);
    remove_device_method_call->SetSerial(kTestSerial);
    dbus::MessageWriter writer(remove_device_method_call);
    writer.AppendObjectPath(dbus::ObjectPath(device_object_path));
  }

  base::MessageLoop message_loop_;
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> bluez_object_proxy_;
  scoped_refptr<dbus::MockObjectManager> bluez_object_manager_;
  // Declare mock_exported_objects_ before newblue_daemon_ to make sure the
  // MockExportedObject-s are destroyed after newblue_daemon_.
  std::map<dbus::ObjectPath, scoped_refptr<dbus::MockExportedObject>>
      mock_exported_objects_;
  std::unique_ptr<NewblueDaemon> newblue_daemon_;
  MockLibNewblue* libnewblue_;
  dbus::ExportedObject::MethodCallCallback dummy_method_handler_;
};

TEST_F(NewblueDaemonTest, InitFailed) {
  scoped_refptr<dbus::MockExportedObject> exported_root_object =
      SetupExportedRootObject();
  scoped_refptr<dbus::MockExportedObject> exported_agent_manager_object =
      SetupExportedAgentManagerObject();
  scoped_refptr<dbus::MockExportedObject> exported_adapter_object =
      SetupExportedAdapterObject();
  ExpectPropertiesMethodsExportedSync(exported_adapter_object);
  ExpectAdvertisingManagerMethodsExported(exported_adapter_object);
  ExpectPropertiesMethodsExportedSync(exported_agent_manager_object);
  ExpectAgentManagerMethodsExported(exported_agent_manager_object);

  ExpectTestInit(exported_root_object);

  EXPECT_CALL(*libnewblue_, HciUp(_, _, _)).WillOnce(Return(false));
  EXPECT_FALSE(newblue_daemon_->Init(
      bus_, nullptr /* no need to access the delegator */));

  // Shutdown now to make sure ExportedObjectManagerWrapper is destructed first
  // before the mocked objects.
  newblue_daemon_->Shutdown();
}

TEST_F(NewblueDaemonTest, InitSuccessAndBringUp) {
  scoped_refptr<dbus::MockExportedObject> exported_root_object =
      SetupExportedRootObject();
  scoped_refptr<dbus::MockExportedObject> exported_agent_manager_object =
      SetupExportedAgentManagerObject();
  scoped_refptr<dbus::MockExportedObject> exported_adapter_object =
      SetupExportedAdapterObject();
  ExpectPropertiesMethodsExportedSync(exported_adapter_object);
  ExpectAdvertisingManagerMethodsExported(exported_adapter_object);
  ExpectPropertiesMethodsExportedSync(exported_agent_manager_object);
  ExpectAgentManagerMethodsExported(exported_agent_manager_object);

  TestInit(exported_root_object);

  MethodHandlerMap adapter_method_handlers;
  TestAdapterBringUp(exported_adapter_object, adapter_method_handlers);

  EXPECT_CALL(*exported_adapter_object, Unregister()).Times(1);
  EXPECT_CALL(*exported_root_object, Unregister()).Times(1);
  // Shutdown now to make sure ExportedObjectManagerWrapper is destructed first
  // before the mocked objects.
  newblue_daemon_->Shutdown();
}

TEST_F(NewblueDaemonTest, DiscoveryAPI) {
  scoped_refptr<dbus::MockExportedObject> exported_root_object =
      SetupExportedRootObject();
  scoped_refptr<dbus::MockExportedObject> exported_agent_manager_object =
      SetupExportedAgentManagerObject();
  scoped_refptr<dbus::MockExportedObject> exported_adapter_object =
      SetupExportedAdapterObject();
  ExpectPropertiesMethodsExportedSync(exported_adapter_object);
  ExpectAdvertisingManagerMethodsExported(exported_adapter_object);
  ExpectPropertiesMethodsExportedSync(exported_agent_manager_object);
  ExpectAgentManagerMethodsExported(exported_agent_manager_object);

  TestInit(exported_root_object);

  dbus::ExportedObject::MethodCallCallback start_discovery_handler;
  dbus::ExportedObject::MethodCallCallback stop_discovery_handler;
  dbus::ExportedObject::MethodCallCallback remove_device_handler;
  MethodHandlerMap adapter_method_handlers;
  adapter_method_handlers[bluetooth_adapter::kStartDiscovery] =
      &start_discovery_handler;
  adapter_method_handlers[bluetooth_adapter::kStopDiscovery] =
      &stop_discovery_handler;
  adapter_method_handlers[bluetooth_adapter::kRemoveDevice] =
      &remove_device_handler;
  TestAdapterBringUp(exported_adapter_object, adapter_method_handlers);

  ASSERT_FALSE(start_discovery_handler.is_null());
  ASSERT_FALSE(stop_discovery_handler.is_null());
  ASSERT_FALSE(remove_device_handler.is_null());

  // StartDiscovery by the first client, it should return D-Bus success and
  // should trigger NewBlue StartDiscovery.
  dbus::MethodCall start_discovery_method_call(
      bluetooth_adapter::kBluetoothAdapterInterface,
      bluetooth_adapter::kStartDiscovery);
  start_discovery_method_call.SetPath(dbus::ObjectPath(kAdapterObjectPath));
  start_discovery_method_call.SetSender(kTestSender);
  start_discovery_method_call.SetSerial(kTestSerial);
  std::unique_ptr<dbus::Response> start_discovery_response;
  Newblue::DeviceDiscoveredCallback on_device_discovered;
  hciDeviceDiscoveredLeCbk inquiry_response_callback;
  void* inquiry_response_callback_data;
  EXPECT_CALL(*libnewblue_, HciDiscoverLeStart(_, _, /* active */ true,
                                               /* use_random_addr */ false))
      .WillOnce(DoAll(SaveArg<0>(&inquiry_response_callback),
                      SaveArg<1>(&inquiry_response_callback_data),
                      Return(kTestDiscoveryId)));
  start_discovery_handler.Run(
      &start_discovery_method_call,
      base::Bind(&SaveResponse, &start_discovery_response));
  EXPECT_EQ("", start_discovery_response->GetErrorName());
  ASSERT_NE(nullptr, inquiry_response_callback);
  ASSERT_NE(nullptr, inquiry_response_callback_data);
  // StartDiscovery again by the same client, it should return D-Bus error and
  // should not affect NewBlue discovery state.
  EXPECT_CALL(*libnewblue_, HciDiscoverLeStart(_, _, _, _)).Times(0);
  start_discovery_handler.Run(
      &start_discovery_method_call,
      base::Bind(&SaveResponse, &start_discovery_response));
  EXPECT_EQ(bluetooth_adapter::kErrorInProgress,
            start_discovery_response->GetErrorName());
  // StartDiscovery by a different client, it should return D-Bus success and
  // should not affect NewBlue discovery state since it has already been
  // started.
  start_discovery_method_call.SetSender(kTestSender2);
  EXPECT_CALL(*libnewblue_, HciDiscoverLeStart(_, _, _, _)).Times(0);
  start_discovery_handler.Run(
      &start_discovery_method_call,
      base::Bind(&SaveResponse, &start_discovery_response));
  EXPECT_EQ("", start_discovery_response->GetErrorName());

  // Device discovered.
  ExpectDeviceObjectExported(dbus::ObjectPath(kTestDeviceObjectPath));
  struct bt_addr address;
  ConvertToBtAddr(false, kTestDeviceAddress, &address);
  inquiry_response_callback(inquiry_response_callback_data, &address,
                            /* rssi */ -101, HCI_ADV_TYPE_SCAN_RSP,
                            /* eir */ {},
                            /* eir_len*/ 0);
  // Trigger the queued inquiry_response_callback task.
  base::RunLoop().RunUntilIdle();

  // RemoveDevice failed (unknown device address).
  dbus::MethodCall remove_device_method_call(
      bluetooth_adapter::kBluetoothAdapterInterface,
      bluetooth_adapter::kRemoveDevice);
  ConstructRemoveDeviceMethodCall(&remove_device_method_call,
                                  "/org/bluez/hci0/dev_11_11_11_11_11_11");
  std::unique_ptr<dbus::Response> remove_device_response;
  remove_device_handler.Run(&remove_device_method_call,
                            base::Bind(&SaveResponse, &remove_device_response));
  EXPECT_EQ(bluetooth_adapter::kErrorFailed,
            remove_device_response->GetErrorName());
  // RemoveDevice successful.
  dbus::MethodCall remove_device_method_call2(
      bluetooth_adapter::kBluetoothAdapterInterface,
      bluetooth_adapter::kRemoveDevice);
  ConstructRemoveDeviceMethodCall(&remove_device_method_call2,
                                  kTestDeviceObjectPath);
  ExpectDeviceObjectUnexported(dbus::ObjectPath(kTestDeviceObjectPath));
  std::unique_ptr<dbus::Response> remove_device_response2;
  remove_device_handler.Run(
      &remove_device_method_call2,
      base::Bind(&SaveResponse, &remove_device_response2));
  EXPECT_EQ("", remove_device_response2->GetErrorName());

  // StopDiscovery by the first client, it should return D-Bus success and
  // should not affect NewBlue discovery state since there is still another
  // client having discovery session.
  dbus::MethodCall stop_discovery_method_call(
      bluetooth_adapter::kBluetoothAdapterInterface,
      bluetooth_adapter::kStopDiscovery);
  stop_discovery_method_call.SetPath(dbus::ObjectPath(kAdapterObjectPath));
  stop_discovery_method_call.SetSender(kTestSender);
  stop_discovery_method_call.SetSerial(kTestSerial);
  std::unique_ptr<dbus::Response> stop_discovery_response;
  EXPECT_CALL(*libnewblue_, HciDiscoverLeStop(_)).Times(0);
  stop_discovery_handler.Run(
      &stop_discovery_method_call,
      base::Bind(&SaveResponse, &stop_discovery_response));
  EXPECT_EQ("", stop_discovery_response->GetErrorName());
  // StopDiscovery again by the same client, it should return D-Bus error, and
  // should not affect the NewBlue discovery state.
  EXPECT_CALL(*libnewblue_, HciDiscoverLeStop(_)).Times(0);
  stop_discovery_handler.Run(
      &stop_discovery_method_call,
      base::Bind(&SaveResponse, &stop_discovery_response));
  EXPECT_EQ(bluetooth_adapter::kErrorFailed,
            stop_discovery_response->GetErrorName());
  // StopDiscovery by the other client, it should return D-Bus success, and
  // should trigger NewBlue's StopDiscovery since there is no more client having
  // a discovery session.
  stop_discovery_method_call.SetSender(kTestSender2);
  EXPECT_CALL(*libnewblue_, HciDiscoverLeStop(kTestDiscoveryId))
      .WillOnce(Return(true));
  stop_discovery_handler.Run(
      &stop_discovery_method_call,
      base::Bind(&SaveResponse, &stop_discovery_response));
  EXPECT_EQ("", stop_discovery_response->GetErrorName());

  EXPECT_CALL(*exported_adapter_object, Unregister()).Times(1);
  EXPECT_CALL(*exported_root_object, Unregister()).Times(1);
  // Shutdown now to make sure ExportedObjectManagerWrapper is destructed first
  // before the mocked objects.
  newblue_daemon_->Shutdown();
}

TEST_F(NewblueDaemonTest, IdleMode) {
  EXPECT_CALL(*bus_,
              RequestOwnershipAndBlock(
                  newblue_object_manager::kNewblueObjectManagerServiceName,
                  dbus::Bus::REQUIRE_PRIMARY))
      .WillOnce(Return(true));

  auto libnewblue = std::make_unique<MockLibNewblue>();
  libnewblue_ = libnewblue.get();
  auto newblue = std::make_unique<Newblue>(std::move(libnewblue));
  newblue_daemon_ = std::make_unique<NewblueDaemon>(std::move(newblue),
                                                    true /* is_idle_mode */);

  // In idle mode, the daemon shouldn't try to bring up the LE stack.
  EXPECT_CALL(*libnewblue_, HciUp(_, _, _)).Times(0);
  EXPECT_TRUE(newblue_daemon_->Init(
      bus_, nullptr /* no need to access the delegator */));

  // Shutdown now to make sure ExportedObjectManagerWrapper is destructed first
  // before the mocked objects.
  newblue_daemon_->Shutdown();
}

}  // namespace bluetooth
