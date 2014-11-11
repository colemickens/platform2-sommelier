// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/third_party_vpn_driver.h"

#include <gtest/gtest.h>

#include "shill/mock_adaptors.h"
#include "shill/mock_device_info.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_file_io.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_service.h"
#include "shill/mock_store.h"
#include "shill/mock_virtual_device.h"
#include "shill/mock_vpn_service.h"
#include "shill/nice_mock_control.h"

using testing::_;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::SetArgumentPointee;

namespace shill {

class ThirdPartyVpnDriverTest : public testing::Test {
 public:
  ThirdPartyVpnDriverTest()
      : device_info_(&control_, &dispatcher_, &metrics_, &manager_),
        metrics_(&dispatcher_),
        manager_(&control_, &dispatcher_, &metrics_, &glib_),
        driver_(new ThirdPartyVpnDriver(&control_, &dispatcher_, &metrics_,
                                        &manager_, &device_info_)),
        adaptor_interface_(new ThirdPartyVpnMockAdaptor()),
        service_(new MockVPNService(&control_, &dispatcher_, &metrics_,
                                    &manager_, driver_)),
        device_(new MockVirtualDevice(&control_, &dispatcher_, &metrics_,
                                      &manager_, kInterfaceName,
                                      kInterfaceIndex, Technology::kVPN)) {}

  virtual ~ThirdPartyVpnDriverTest() {}

  virtual void SetUp() {
    driver_->adaptor_interface_.reset(adaptor_interface_);
    driver_->file_io_ = &mock_file_io_;
  }

  virtual void TearDown() {
    driver_->device_ = nullptr;
    driver_->service_ = nullptr;
    driver_->file_io_ = nullptr;
  }

 protected:
  static const char kConfigName[];
  static const char kInterfaceName[];
  static const int kInterfaceIndex;

  NiceMockControl control_;
  NiceMock<MockDeviceInfo> device_info_;
  MockEventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockFileIO mock_file_io_;
  MockGLib glib_;
  MockManager manager_;
  ThirdPartyVpnDriver *driver_;                  // Owned by |service_|
  ThirdPartyVpnMockAdaptor *adaptor_interface_;  // Owned by |driver_|
  scoped_refptr<MockVPNService> service_;
  scoped_refptr<MockVirtualDevice> device_;
};

const char ThirdPartyVpnDriverTest::kConfigName[] = "default-1";
const char ThirdPartyVpnDriverTest::kInterfaceName[] = "tun0";
const int ThirdPartyVpnDriverTest::kInterfaceIndex = 123;

TEST_F(ThirdPartyVpnDriverTest, ConnectAndDisconnect) {
  const std::string interface = kInterfaceName;
  IOHandler *io_handler = new IOHandler();  // Owned by |driver_|
  int fd = 1;

  EXPECT_CALL(*service_, SetState(Service::kStateConfiguring)).Times(1);
  EXPECT_CALL(device_info_, CreateTunnelInterface(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(interface), Return(true)));
  Error error;
  driver_->Connect(service_, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(kInterfaceName, driver_->tunnel_interface_);
  EXPECT_TRUE(driver_->IsConnectTimeoutStarted());

  EXPECT_CALL(device_info_, OpenTunnelInterface(interface))
      .WillOnce(Return(fd));
  EXPECT_CALL(dispatcher_, CreateInputHandler(fd, _, _))
      .WillOnce(Return(io_handler));
  EXPECT_CALL(*adaptor_interface_, EmitPlatformMessage(static_cast<uint32_t>(
                                       ThirdPartyVpnDriver::kConnected)));
  EXPECT_FALSE(driver_->ClaimInterface("eth1", kInterfaceIndex));
  EXPECT_TRUE(driver_->ClaimInterface(interface, kInterfaceIndex));
  EXPECT_EQ(driver_->active_client_, driver_);
  EXPECT_TRUE(driver_->parameters_expected_);
  EXPECT_EQ(driver_->io_handler_.get(), io_handler);
  ASSERT_TRUE(driver_->device_);
  EXPECT_EQ(kInterfaceIndex, driver_->device_->interface_index());

  EXPECT_CALL(*service_, SetState(Service::kStateIdle)).Times(1);
  EXPECT_CALL(*adaptor_interface_, EmitPlatformMessage(static_cast<uint32_t>(
                                       ThirdPartyVpnDriver::kDisconnected)));
  EXPECT_CALL(mock_file_io_, Close(fd));
  driver_->Disconnect();
  EXPECT_EQ(driver_->io_handler_.get(), nullptr);
}

TEST_F(ThirdPartyVpnDriverTest, SendPacket) {
  int fd = 1;
  std::string error;
  std::vector<uint8_t> ip_packet(5, 0);
  driver_->SendPacket(ip_packet, &error);
  EXPECT_EQ(error, "Unexpected call");

  error.clear();
  ThirdPartyVpnDriver::active_client_ = driver_;
  driver_->SendPacket(ip_packet, &error);
  EXPECT_EQ(error, "Device not open");

  driver_->tun_fd_ = fd;
  error.clear();
  EXPECT_CALL(mock_file_io_, Write(fd, ip_packet.data(), ip_packet.size()))
      .WillOnce(Return(ip_packet.size() - 1));
  EXPECT_CALL(
      *adaptor_interface_,
      EmitPlatformMessage(static_cast<uint32_t>(ThirdPartyVpnDriver::kError)));
  driver_->SendPacket(ip_packet, &error);
  EXPECT_EQ(error, "Partial write");

  error.clear();
  EXPECT_CALL(mock_file_io_, Write(fd, ip_packet.data(), ip_packet.size()))
      .WillOnce(Return(ip_packet.size()));
  driver_->SendPacket(ip_packet, &error);
  EXPECT_TRUE(error.empty());

  driver_->tun_fd_ = -1;

  EXPECT_CALL(*adaptor_interface_, EmitPlatformMessage(static_cast<uint32_t>(
                                       ThirdPartyVpnDriver::kDisconnected)));
}

TEST_F(ThirdPartyVpnDriverTest, UpdateConnectionState) {
  std::string error;
  driver_->UpdateConnectionState(Service::kStateConfiguring, &error);
  EXPECT_EQ(error, "Unexpected call");

  error.clear();
  ThirdPartyVpnDriver::active_client_ = driver_;
  driver_->UpdateConnectionState(Service::kStateConfiguring, &error);
  EXPECT_EQ(error, "Invalid argument");

  error.clear();
  driver_->service_ = service_;
  EXPECT_CALL(*service_, SetState(Service::kStateConnected)).Times(1);
  driver_->UpdateConnectionState(Service::kStateConnected, &error);
  EXPECT_TRUE(error.empty());
}

TEST_F(ThirdPartyVpnDriverTest, SetParameters) {
  std::map<std::string, std::string> parameters;
  std::string error;
  driver_->SetParameters(parameters, &error);
  EXPECT_EQ(error, "Unexpected call");

  error.clear();
  ThirdPartyVpnDriver::active_client_ = driver_;
  driver_->parameters_expected_ = true;
  driver_->SetParameters(parameters, &error);
  EXPECT_EQ(error,
            "address is missing;subnet_prefix is missing;"
            "dns_servers is missing;bypass_tunnel_for_ip is missing;");

  error.clear();
  parameters["address"] = "1234.1.1.1";
  driver_->SetParameters(parameters, &error);
  EXPECT_EQ(error,
            "address is not a valid IP;subnet_prefix is missing;"
            "dns_servers is missing;bypass_tunnel_for_ip is missing;");

  error.clear();
  parameters["address"] = "123.211.21.18";
  driver_->SetParameters(parameters, &error);
  EXPECT_EQ(error,
            "subnet_prefix is missing;dns_servers is missing;"
            "bypass_tunnel_for_ip is missing;");

  error.clear();
  parameters["bypass_tunnel_for_ip"] = "1234.1.1.1";
  driver_->SetParameters(parameters, &error);
  EXPECT_EQ(error,
            "subnet_prefix is missing;dns_servers is missing;"
            "bypass_tunnel_for_ip has no valid values or is empty;");

  error.clear();
  parameters["bypass_tunnel_for_ip"] = "123.211.21.18";
  driver_->SetParameters(parameters, &error);
  EXPECT_EQ(error, "subnet_prefix is missing;dns_servers is missing;");

  error.clear();
  parameters["subnet_prefix"] = "123";
  driver_->SetParameters(parameters, &error);
  EXPECT_EQ(error,
            "subnet_prefix not in expected range;dns_servers is missing;");

  error.clear();
  parameters["subnet_prefix"] = "12";
  driver_->SetParameters(parameters, &error);
  EXPECT_EQ(error, "dns_servers is missing;");

  error.clear();
  parameters["dns_servers"] = "12 123123 43902374";
  driver_->SetParameters(parameters, &error);
  EXPECT_EQ(error, "dns_servers has no valid values or is empty;");

  driver_->device_ =
      new MockVirtualDevice(&control_, &dispatcher_, &metrics_, &manager_,
                            kInterfaceName, kInterfaceIndex, Technology::kVPN);
  error.clear();
  parameters["dns_servers"] = "123.211.21.18 123.211.21.19";
  driver_->SetParameters(parameters, &error);
  EXPECT_TRUE(error.empty());
  EXPECT_FALSE(driver_->parameters_expected_);
  driver_->device_ = nullptr;
}

}  // namespace shill
