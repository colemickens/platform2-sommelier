// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/device_info.h"

#include <glib.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/netlink.h>  // Needs typedefs from sys/socket.h.
#include <linux/rtnetlink.h>
#include <net/if_arp.h>

#include <base/file_util.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop.h>
#include <base/scoped_temp_dir.h>
#include <base/stl_util.h>
#include <base/string_number_conversions.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/ip_address.h"
#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_modem_info.h"
#include "shill/mock_routing_table.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/mock_sockets.h"
#include "shill/mock_vpn_provider.h"
#include "shill/mock_wimax_provider.h"
#include "shill/rtnl_message.h"

using base::Callback;
using std::map;
using std::string;
using std::vector;
using testing::_;
using testing::Mock;
using testing::Return;
using testing::StrictMock;
using testing::Test;

namespace shill {

class TestEventDispatcher : public EventDispatcher {
 public:
  virtual IOHandler *CreateInputHandler(
      int /*fd*/,
      const Callback<void(InputData*)> &/*callback*/) {
    return NULL;
  }
};

class DeviceInfoTest : public Test {
 public:
  DeviceInfoTest()
      : manager_(&control_interface_, &dispatcher_, &metrics_, &glib_),
        device_info_(&control_interface_, &dispatcher_, &metrics_, &manager_) {
  }
  virtual ~DeviceInfoTest() {}

  virtual void SetUp() {
    device_info_.rtnl_handler_ = &rtnl_handler_;
    device_info_.routing_table_ = &routing_table_;
  }

  IPAddress CreateInterfaceAddress() {
    // Create an IP address entry (as if left-over from a previous connection
    // manager).
    IPAddress address(IPAddress::kFamilyIPv4);
    EXPECT_TRUE(address.SetAddressFromString(kTestIPAddress0));
    address.set_prefix(kTestIPAddressPrefix0);
    vector<DeviceInfo::AddressData> &addresses =
        device_info_.infos_[kTestDeviceIndex].ip_addresses;
    addresses.push_back(DeviceInfo::AddressData(address, 0, RT_SCOPE_UNIVERSE));
    EXPECT_EQ(1, addresses.size());
    return address;
  }

  DeviceRefPtr CreateDevice(const std::string &link_name,
                            const std::string &address,
                            int interface_index,
                            Technology::Identifier technology) {
    return device_info_.CreateDevice(link_name, address, interface_index,
                                     technology);
  }


 protected:
  static const int kTestDeviceIndex;
  static const char kTestDeviceName[];
  static const char kTestMACAddress[];
  static const char kTestIPAddress0[];
  static const int kTestIPAddressPrefix0;
  static const char kTestIPAddress1[];
  static const int kTestIPAddressPrefix1;
  static const char kTestIPAddress2[];
  static const char kTestIPAddress3[];
  static const char kTestIPAddress4[];

  RTNLMessage *BuildLinkMessage(RTNLMessage::Mode mode);
  RTNLMessage *BuildLinkMessageWithInterfaceName(RTNLMessage::Mode mode,
                                                 const string &interface_name);
  RTNLMessage *BuildAddressMessage(RTNLMessage::Mode mode,
                                   const IPAddress &address,
                                   unsigned char flags,
                                   unsigned char scope);
  void SendMessageToDeviceInfo(const RTNLMessage &message);

  MockGLib glib_;
  MockControl control_interface_;
  MockMetrics metrics_;
  StrictMock<MockManager> manager_;
  DeviceInfo device_info_;
  TestEventDispatcher dispatcher_;
  MockRoutingTable routing_table_;
  StrictMock<MockRTNLHandler> rtnl_handler_;
};

const int DeviceInfoTest::kTestDeviceIndex = 123456;
const char DeviceInfoTest::kTestDeviceName[] = "test-device";
const char DeviceInfoTest::kTestMACAddress[] = {
  0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff };
const char DeviceInfoTest::kTestIPAddress0[] = "192.168.1.1";
const int DeviceInfoTest::kTestIPAddressPrefix0 = 24;
const char DeviceInfoTest::kTestIPAddress1[] = "fe80::1aa9:5ff:abcd:1234";
const int DeviceInfoTest::kTestIPAddressPrefix1 = 64;
const char DeviceInfoTest::kTestIPAddress2[] = "fe80::1aa9:5ff:abcd:1235";
const char DeviceInfoTest::kTestIPAddress3[] = "fe80::1aa9:5ff:abcd:1236";
const char DeviceInfoTest::kTestIPAddress4[] = "fe80::1aa9:5ff:abcd:1237";

RTNLMessage *DeviceInfoTest::BuildLinkMessageWithInterfaceName(
    RTNLMessage::Mode mode, const string &interface_name) {
  RTNLMessage *message = new RTNLMessage(
      RTNLMessage::kTypeLink,
      mode,
      0,
      0,
      0,
      kTestDeviceIndex,
      IPAddress::kFamilyIPv4);
  message->SetAttribute(static_cast<uint16>(IFLA_IFNAME),
                        ByteString(interface_name, true));
  ByteString test_address(kTestMACAddress, sizeof(kTestMACAddress));
  message->SetAttribute(IFLA_ADDRESS, test_address);
  return message;
}

RTNLMessage *DeviceInfoTest::BuildLinkMessage(RTNLMessage::Mode mode) {
  return BuildLinkMessageWithInterfaceName(mode, kTestDeviceName);
}

RTNLMessage *DeviceInfoTest::BuildAddressMessage(RTNLMessage::Mode mode,
                                                 const IPAddress &address,
                                                 unsigned char flags,
                                                 unsigned char scope) {
  RTNLMessage *message = new RTNLMessage(
      RTNLMessage::kTypeAddress,
      mode,
      0,
      0,
      0,
      kTestDeviceIndex,
      address.family());
  message->SetAttribute(IFA_ADDRESS, address.address());
  message->set_address_status(
      RTNLMessage::AddressStatus(address.prefix(), flags, scope));
  return message;
}

void DeviceInfoTest::SendMessageToDeviceInfo(const RTNLMessage &message) {
  if (message.type() == RTNLMessage::kTypeLink) {
    device_info_.LinkMsgHandler(message);
  } else if (message.type() == RTNLMessage::kTypeAddress) {
    device_info_.AddressMsgHandler(message);
  } else {
    NOTREACHED();
  }
}

MATCHER_P(IsIPAddress, address, "") {
  // NB: IPAddress objects don't support the "==" operator as per style, so
  // we need a custom matcher.
  return address.Equals(arg);
}

TEST_F(DeviceInfoTest, StartStop) {
  EXPECT_FALSE(device_info_.link_listener_.get());
  EXPECT_FALSE(device_info_.address_listener_.get());
  EXPECT_TRUE(device_info_.infos_.empty());

  EXPECT_CALL(rtnl_handler_, RequestDump(RTNLHandler::kRequestLink |
                                         RTNLHandler::kRequestAddr));
  device_info_.Start();
  EXPECT_TRUE(device_info_.link_listener_.get());
  EXPECT_TRUE(device_info_.address_listener_.get());
  EXPECT_TRUE(device_info_.infos_.empty());
  Mock::VerifyAndClearExpectations(&rtnl_handler_);

  CreateInterfaceAddress();
  EXPECT_FALSE(device_info_.infos_.empty());

  device_info_.Stop();
  EXPECT_FALSE(device_info_.link_listener_.get());
  EXPECT_FALSE(device_info_.address_listener_.get());
  EXPECT_TRUE(device_info_.infos_.empty());
}

TEST_F(DeviceInfoTest, DeviceEnumeration) {
  scoped_ptr<RTNLMessage> message(BuildLinkMessage(RTNLMessage::kModeAdd));
  message->set_link_status(RTNLMessage::LinkStatus(0, IFF_LOWER_UP, 0));
  EXPECT_FALSE(device_info_.GetDevice(kTestDeviceIndex).get());
  EXPECT_EQ(-1, device_info_.GetIndex(kTestDeviceName));
  SendMessageToDeviceInfo(*message);
  EXPECT_TRUE(device_info_.GetDevice(kTestDeviceIndex).get());
  unsigned int flags = 0;
  EXPECT_TRUE(device_info_.GetFlags(kTestDeviceIndex, &flags));
  EXPECT_EQ(IFF_LOWER_UP, flags);
  ByteString address;
  EXPECT_TRUE(device_info_.GetMACAddress(kTestDeviceIndex, &address));
  EXPECT_FALSE(address.IsEmpty());
  EXPECT_TRUE(address.Equals(ByteString(kTestMACAddress,
                                        sizeof(kTestMACAddress))));
  EXPECT_EQ(kTestDeviceIndex, device_info_.GetIndex(kTestDeviceName));

  message.reset(BuildLinkMessage(RTNLMessage::kModeAdd));
  message->set_link_status(RTNLMessage::LinkStatus(0, IFF_UP | IFF_RUNNING, 0));
  SendMessageToDeviceInfo(*message);
  EXPECT_TRUE(device_info_.GetFlags(kTestDeviceIndex, &flags));
  EXPECT_EQ(IFF_UP | IFF_RUNNING, flags);

  message.reset(BuildLinkMessage(RTNLMessage::kModeDelete));
  EXPECT_CALL(manager_, DeregisterDevice(_)).Times(1);
  SendMessageToDeviceInfo(*message);
  EXPECT_FALSE(device_info_.GetDevice(kTestDeviceIndex).get());
  EXPECT_FALSE(device_info_.GetFlags(kTestDeviceIndex, NULL));
  EXPECT_EQ(-1, device_info_.GetIndex(kTestDeviceName));
}

TEST_F(DeviceInfoTest, CreateDeviceCellular) {
  IPAddress address = CreateInterfaceAddress();

  // A cellular device should be offered to ModemInfo.
  StrictMock<MockModemInfo> modem_info;
  EXPECT_CALL(manager_, modem_info()).WillOnce(Return(&modem_info));
  EXPECT_CALL(modem_info, OnDeviceInfoAvailable(kTestDeviceName)).Times(1);
  EXPECT_CALL(routing_table_, FlushRoutes(kTestDeviceIndex)).Times(1);
  EXPECT_CALL(rtnl_handler_, RemoveInterfaceAddress(kTestDeviceIndex,
                                                    IsIPAddress(address)));
  EXPECT_FALSE(CreateDevice(
      kTestDeviceName, "address", kTestDeviceIndex, Technology::kCellular));
}

TEST_F(DeviceInfoTest, CreateDeviceWiMax) {
  IPAddress address = CreateInterfaceAddress();

  // A WiMax device should be offered to WiMaxProvider.
  StrictMock<MockWiMaxProvider> wimax_provider;
  EXPECT_CALL(manager_, wimax_provider()).WillOnce(Return(&wimax_provider));
  EXPECT_CALL(wimax_provider, OnDeviceInfoAvailable(kTestDeviceName)).Times(1);
  EXPECT_CALL(routing_table_, FlushRoutes(kTestDeviceIndex)).Times(1);
  EXPECT_CALL(rtnl_handler_, RemoveInterfaceAddress(kTestDeviceIndex,
                                                    IsIPAddress(address)));
  EXPECT_FALSE(CreateDevice(
      kTestDeviceName, "address", kTestDeviceIndex, Technology::kWiMax));
}

TEST_F(DeviceInfoTest, CreateDeviceEthernet) {
  IPAddress address = CreateInterfaceAddress();

  // An Ethernet device should cause routes and addresses to be flushed.
  EXPECT_CALL(routing_table_, FlushRoutes(kTestDeviceIndex)).Times(1);
  EXPECT_CALL(rtnl_handler_, RemoveInterfaceAddress(kTestDeviceIndex,
                                                    IsIPAddress(address)));
  DeviceRefPtr device = CreateDevice(
      kTestDeviceName, "address", kTestDeviceIndex, Technology::kEthernet);
  EXPECT_TRUE(device);
  Mock::VerifyAndClearExpectations(&routing_table_);
  Mock::VerifyAndClearExpectations(&rtnl_handler_);

  // The Ethernet device destructor notifies the manager.
  EXPECT_CALL(manager_, UpdateEnabledTechnologies()).Times(1);
  device = NULL;
}

TEST_F(DeviceInfoTest, CreateDeviceVirtioEthernet) {
  IPAddress address = CreateInterfaceAddress();

  // VirtioEthernet is identical to Ethernet from the perspective of this test.
  EXPECT_CALL(routing_table_, FlushRoutes(kTestDeviceIndex)).Times(1);
  EXPECT_CALL(rtnl_handler_, RemoveInterfaceAddress(kTestDeviceIndex,
                                                    IsIPAddress(address)));
  DeviceRefPtr device = CreateDevice(
      kTestDeviceName, "address", kTestDeviceIndex,
      Technology::kVirtioEthernet);
  EXPECT_TRUE(device);
  Mock::VerifyAndClearExpectations(&routing_table_);
  Mock::VerifyAndClearExpectations(&rtnl_handler_);

  // The Ethernet device destructor notifies the manager.
  EXPECT_CALL(manager_, UpdateEnabledTechnologies()).Times(1);
  device = NULL;
}

TEST_F(DeviceInfoTest, CreateDeviceWiFi) {
  IPAddress address = CreateInterfaceAddress();

  // WiFi looks a lot like Ethernet too.
  EXPECT_CALL(routing_table_, FlushRoutes(kTestDeviceIndex)).Times(1);
  EXPECT_CALL(rtnl_handler_, RemoveInterfaceAddress(kTestDeviceIndex,
                                                    IsIPAddress(address)));
  EXPECT_TRUE(CreateDevice(
      kTestDeviceName, "address", kTestDeviceIndex, Technology::kWifi));
}

TEST_F(DeviceInfoTest, CreateDeviceTunnelAccepted) {
  IPAddress address = CreateInterfaceAddress();

  // A VPN device should be offered to VPNProvider.
  StrictMock<MockVPNProvider> vpn_provider;
  EXPECT_CALL(manager_, vpn_provider()).WillOnce(Return(&vpn_provider));
  EXPECT_CALL(vpn_provider,
              OnDeviceInfoAvailable(kTestDeviceName, kTestDeviceIndex))
      .WillOnce(Return(true));
  EXPECT_CALL(routing_table_, FlushRoutes(kTestDeviceIndex)).Times(1);
  EXPECT_CALL(rtnl_handler_, RemoveInterfaceAddress(kTestDeviceIndex,
                                                    IsIPAddress(address)));
  EXPECT_CALL(rtnl_handler_, RemoveInterface(_)).Times(0);
  EXPECT_FALSE(CreateDevice(
      kTestDeviceName, "address", kTestDeviceIndex, Technology::kTunnel));
}

TEST_F(DeviceInfoTest, CreateDeviceTunnelRejected) {
  IPAddress address = CreateInterfaceAddress();

  // A VPN device should be offered to VPNProvider.
  StrictMock<MockVPNProvider> vpn_provider;
  EXPECT_CALL(manager_, vpn_provider()).WillOnce(Return(&vpn_provider));
  EXPECT_CALL(vpn_provider,
              OnDeviceInfoAvailable(kTestDeviceName, kTestDeviceIndex))
      .WillOnce(Return(false));
  EXPECT_CALL(routing_table_, FlushRoutes(kTestDeviceIndex)).Times(1);
  EXPECT_CALL(rtnl_handler_, RemoveInterfaceAddress(kTestDeviceIndex,
                                                    IsIPAddress(address)));
  // Since the device was rejected by the VPNProvider, DeviceInfo will
  // remove the interface.
  EXPECT_CALL(rtnl_handler_, RemoveInterface(kTestDeviceIndex)).Times(1);
  EXPECT_FALSE(CreateDevice(
      kTestDeviceName, "address", kTestDeviceIndex, Technology::kTunnel));
}

TEST_F(DeviceInfoTest, CreateDevicePPP) {
  IPAddress address = CreateInterfaceAddress();

  // A VPN device should be offered to VPNProvider.
  StrictMock<MockVPNProvider> vpn_provider;
  EXPECT_CALL(manager_, vpn_provider()).WillOnce(Return(&vpn_provider));
  EXPECT_CALL(vpn_provider,
              OnDeviceInfoAvailable(kTestDeviceName, kTestDeviceIndex))
      .WillOnce(Return(false));
  EXPECT_CALL(routing_table_, FlushRoutes(kTestDeviceIndex)).Times(1);
  EXPECT_CALL(rtnl_handler_, RemoveInterfaceAddress(kTestDeviceIndex,
                                                    IsIPAddress(address)));
  // We do not remove PPP interfaces even if the provider does not accept it.
  EXPECT_CALL(rtnl_handler_, RemoveInterface(_)).Times(0);
  EXPECT_FALSE(CreateDevice(
      kTestDeviceName, "address", kTestDeviceIndex, Technology::kPPP));
}

TEST_F(DeviceInfoTest, CreateDeviceLoopback) {
  // A loopback device should be brought up, and nothing else done to it.
  EXPECT_CALL(routing_table_, FlushRoutes(_)).Times(0);
  EXPECT_CALL(rtnl_handler_, RemoveInterfaceAddress(_, _)).Times(0);
  EXPECT_CALL(rtnl_handler_,
              SetInterfaceFlags(kTestDeviceIndex, IFF_UP, IFF_UP)).Times(1);
  EXPECT_FALSE(CreateDevice(
      kTestDeviceName, "address", kTestDeviceIndex, Technology::kLoopback));
}

TEST_F(DeviceInfoTest, CreateDeviceUnknown) {
  IPAddress address = CreateInterfaceAddress();

  // An unknown (blacklisted, unhandled, etc) device won't be flushed or
  // registered.
  EXPECT_CALL(routing_table_, FlushRoutes(_)).Times(0);
  EXPECT_CALL(rtnl_handler_, RemoveInterfaceAddress(_, _)).Times(0);
  EXPECT_TRUE(CreateDevice(
      kTestDeviceName, "address", kTestDeviceIndex, Technology::kUnknown));
}

TEST_F(DeviceInfoTest, DeviceBlackList) {
  device_info_.AddDeviceToBlackList(kTestDeviceName);
  scoped_ptr<RTNLMessage> message(BuildLinkMessage(RTNLMessage::kModeAdd));
  SendMessageToDeviceInfo(*message);

  DeviceRefPtr device = device_info_.GetDevice(kTestDeviceIndex);
  ASSERT_TRUE(device.get());
  EXPECT_TRUE(device->TechnologyIs(Technology::kBlacklisted));
}

TEST_F(DeviceInfoTest, DeviceAddressList) {
  scoped_ptr<RTNLMessage> message(BuildLinkMessage(RTNLMessage::kModeAdd));
  SendMessageToDeviceInfo(*message);

  vector<DeviceInfo::AddressData> addresses;
  EXPECT_TRUE(device_info_.GetAddresses(kTestDeviceIndex, &addresses));
  EXPECT_TRUE(addresses.empty());

  // Add an address to the device address list
  IPAddress ip_address0(IPAddress::kFamilyIPv4);
  EXPECT_TRUE(ip_address0.SetAddressFromString(kTestIPAddress0));
  ip_address0.set_prefix(kTestIPAddressPrefix0);
  message.reset(BuildAddressMessage(RTNLMessage::kModeAdd, ip_address0, 0, 0));
  SendMessageToDeviceInfo(*message);
  EXPECT_TRUE(device_info_.GetAddresses(kTestDeviceIndex, &addresses));
  EXPECT_EQ(1, addresses.size());
  EXPECT_TRUE(ip_address0.Equals(addresses[0].address));

  // Re-adding the same address shouldn't cause the address list to change
  SendMessageToDeviceInfo(*message);
  EXPECT_TRUE(device_info_.GetAddresses(kTestDeviceIndex, &addresses));
  EXPECT_EQ(1, addresses.size());
  EXPECT_TRUE(ip_address0.Equals(addresses[0].address));

  // Adding a new address should expand the list
  IPAddress ip_address1(IPAddress::kFamilyIPv6);
  EXPECT_TRUE(ip_address1.SetAddressFromString(kTestIPAddress1));
  ip_address1.set_prefix(kTestIPAddressPrefix1);
  message.reset(BuildAddressMessage(RTNLMessage::kModeAdd, ip_address1, 0, 0));
  SendMessageToDeviceInfo(*message);
  EXPECT_TRUE(device_info_.GetAddresses(kTestDeviceIndex, &addresses));
  EXPECT_EQ(2, addresses.size());
  EXPECT_TRUE(ip_address0.Equals(addresses[0].address));
  EXPECT_TRUE(ip_address1.Equals(addresses[1].address));

  // Deleting an address should reduce the list
  message.reset(BuildAddressMessage(RTNLMessage::kModeDelete,
                                    ip_address0,
                                    0,
                                    0));
  SendMessageToDeviceInfo(*message);
  EXPECT_TRUE(device_info_.GetAddresses(kTestDeviceIndex, &addresses));
  EXPECT_EQ(1, addresses.size());
  EXPECT_TRUE(ip_address1.Equals(addresses[0].address));

  // Delete last item
  message.reset(BuildAddressMessage(RTNLMessage::kModeDelete,
                                    ip_address1,
                                    0,
                                    0));
  SendMessageToDeviceInfo(*message);
  EXPECT_TRUE(device_info_.GetAddresses(kTestDeviceIndex, &addresses));
  EXPECT_TRUE(addresses.empty());

  // Delete device
  message.reset(BuildLinkMessage(RTNLMessage::kModeDelete));
  EXPECT_CALL(manager_, DeregisterDevice(_)).Times(1);
  SendMessageToDeviceInfo(*message);

  // Should be able to handle message for interface that doesn't exist
  message.reset(BuildAddressMessage(RTNLMessage::kModeAdd, ip_address0, 0, 0));
  SendMessageToDeviceInfo(*message);
  EXPECT_FALSE(device_info_.GetDevice(kTestDeviceIndex).get());
}

TEST_F(DeviceInfoTest, FlushAddressList) {
  scoped_ptr<RTNLMessage> message(BuildLinkMessage(RTNLMessage::kModeAdd));
  SendMessageToDeviceInfo(*message);

  IPAddress address1(IPAddress::kFamilyIPv6);
  EXPECT_TRUE(address1.SetAddressFromString(kTestIPAddress1));
  address1.set_prefix(kTestIPAddressPrefix1);
  message.reset(BuildAddressMessage(RTNLMessage::kModeAdd,
                                    address1,
                                    0,
                                    RT_SCOPE_UNIVERSE));
  SendMessageToDeviceInfo(*message);
  IPAddress address2(IPAddress::kFamilyIPv6);
  EXPECT_TRUE(address2.SetAddressFromString(kTestIPAddress2));
  message.reset(BuildAddressMessage(RTNLMessage::kModeAdd,
                                    address2,
                                    IFA_F_TEMPORARY,
                                    RT_SCOPE_UNIVERSE));
  SendMessageToDeviceInfo(*message);
  IPAddress address3(IPAddress::kFamilyIPv6);
  EXPECT_TRUE(address3.SetAddressFromString(kTestIPAddress3));
  message.reset(BuildAddressMessage(RTNLMessage::kModeAdd,
                                    address3,
                                    0,
                                    RT_SCOPE_LINK));
  SendMessageToDeviceInfo(*message);
  IPAddress address4(IPAddress::kFamilyIPv6);
  EXPECT_TRUE(address4.SetAddressFromString(kTestIPAddress4));
  message.reset(BuildAddressMessage(RTNLMessage::kModeAdd,
                                    address4,
                                    IFA_F_PERMANENT,
                                    RT_SCOPE_UNIVERSE));
  SendMessageToDeviceInfo(*message);

  // DeviceInfo now has 4 addresses associated with it, but only two of
  // them are valid for flush.
  EXPECT_CALL(rtnl_handler_, RemoveInterfaceAddress(kTestDeviceIndex,
                                                    IsIPAddress(address1)));
  EXPECT_CALL(rtnl_handler_, RemoveInterfaceAddress(kTestDeviceIndex,
                                                    IsIPAddress(address2)));
  device_info_.FlushAddresses(kTestDeviceIndex);
}

TEST_F(DeviceInfoTest, HasSubdir) {
  ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  EXPECT_TRUE(file_util::CreateDirectory(temp_dir.path().Append("child1")));
  FilePath child2 = temp_dir.path().Append("child2");
  EXPECT_TRUE(file_util::CreateDirectory(child2));
  FilePath grandchild = child2.Append("grandchild");
  EXPECT_TRUE(file_util::CreateDirectory(grandchild));
  EXPECT_TRUE(file_util::CreateDirectory(grandchild.Append("greatgrandchild")));
  EXPECT_TRUE(DeviceInfo::HasSubdir(temp_dir.path(),
                                    FilePath("grandchild")));
  EXPECT_TRUE(DeviceInfo::HasSubdir(temp_dir.path(),
                                    FilePath("greatgrandchild")));
  EXPECT_FALSE(DeviceInfo::HasSubdir(temp_dir.path(),
                                     FilePath("nonexistent")));
}

class DeviceInfoTechnologyTest : public DeviceInfoTest {
 public:
  DeviceInfoTechnologyTest()
      : DeviceInfoTest(),
        test_device_name_(kTestDeviceName) {}
  virtual ~DeviceInfoTechnologyTest() {}

  virtual void SetUp() {
    temp_dir_.CreateUniqueTempDir();
    device_info_root_ = temp_dir_.path().Append("sys/class/net");
    device_info_.device_info_root_ = device_info_root_;
    // Most tests require that the uevent file exist.
    CreateInfoFile("uevent", "xxx");
  }

  Technology::Identifier GetDeviceTechnology() {
    return device_info_.GetDeviceTechnology(test_device_name_);
  }
  FilePath GetInfoPath(const string &name);
  void CreateInfoFile(const string &name, const string &contents);
  void CreateInfoSymLink(const string &name, const string &contents);
  void SetDeviceName(const string &name) {
    test_device_name_ = name;
    SetUp();
  }

 protected:
  ScopedTempDir temp_dir_;
  FilePath device_info_root_;
  string test_device_name_;
};

FilePath DeviceInfoTechnologyTest::GetInfoPath(const string &name) {
  return device_info_root_.Append(test_device_name_).Append(name);
}

void DeviceInfoTechnologyTest::CreateInfoFile(const string &name,
                                              const string &contents) {
  FilePath info_path = GetInfoPath(name);
  EXPECT_TRUE(file_util::CreateDirectory(info_path.DirName()));
  string contents_newline(contents + "\n");
  EXPECT_TRUE(file_util::WriteFile(info_path, contents_newline.c_str(),
                                   contents_newline.size()));
}

void DeviceInfoTechnologyTest::CreateInfoSymLink(const string &name,
                                                 const string &contents) {
  FilePath info_path = GetInfoPath(name);
  EXPECT_TRUE(file_util::CreateDirectory(info_path.DirName()));
  EXPECT_TRUE(file_util::CreateSymbolicLink(FilePath(contents), info_path));
}

TEST_F(DeviceInfoTechnologyTest, Unknown) {
  EXPECT_EQ(Technology::kUnknown, GetDeviceTechnology());
  // Should still be unknown even without a uevent file.
  EXPECT_TRUE(file_util::Delete(GetInfoPath("uevent"), FALSE));
  EXPECT_EQ(Technology::kUnknown, GetDeviceTechnology());
}

TEST_F(DeviceInfoTechnologyTest, Loopback) {
  CreateInfoFile("type", base::IntToString(ARPHRD_LOOPBACK));
  EXPECT_EQ(Technology::kLoopback, GetDeviceTechnology());
}

TEST_F(DeviceInfoTechnologyTest, PPP) {
  CreateInfoFile("type", base::IntToString(ARPHRD_PPP));
  EXPECT_EQ(Technology::kPPP, GetDeviceTechnology());
}

TEST_F(DeviceInfoTechnologyTest, Tunnel) {
  CreateInfoFile("tun_flags", base::IntToString(IFF_TUN));
  EXPECT_EQ(Technology::kTunnel, GetDeviceTechnology());
}

TEST_F(DeviceInfoTechnologyTest, WiFi) {
  CreateInfoFile("uevent", "DEVTYPE=wlan");
  EXPECT_EQ(Technology::kWifi, GetDeviceTechnology());
  CreateInfoFile("uevent", "foo\nDEVTYPE=wlan");
  EXPECT_EQ(Technology::kWifi, GetDeviceTechnology());
  CreateInfoFile("type", base::IntToString(ARPHRD_IEEE80211_RADIOTAP));
  EXPECT_EQ(Technology::kWiFiMonitor, GetDeviceTechnology());
}

TEST_F(DeviceInfoTechnologyTest, Ethernet) {
  CreateInfoSymLink("device/driver", "xxx");
  EXPECT_EQ(Technology::kEthernet, GetDeviceTechnology());
}

TEST_F(DeviceInfoTechnologyTest, WiMax) {
  CreateInfoSymLink("device/driver", "gdm_wimax");
  EXPECT_EQ(Technology::kWiMax, GetDeviceTechnology());
}

TEST_F(DeviceInfoTechnologyTest, CellularGobi1) {
  CreateInfoSymLink("device/driver", "blah/foo/gobi");
  EXPECT_EQ(Technology::kCellular, GetDeviceTechnology());
}

TEST_F(DeviceInfoTechnologyTest, CellularGobi2) {
  CreateInfoSymLink("device/driver", "../GobiNet");
  EXPECT_EQ(Technology::kCellular, GetDeviceTechnology());
}

TEST_F(DeviceInfoTechnologyTest, QCUSB) {
  CreateInfoSymLink("device/driver", "QCUSBNet2k");
  EXPECT_EQ(Technology::kCellular, GetDeviceTechnology());
}

// Modem with absolute driver path with top-level tty file:
//   /sys/class/net/dev0/device -> /sys/devices/virtual/0/00
//   /sys/devices/virtual/0/00/driver -> /drivers/cdc_ether
//   /sys/devices/virtual/0/01/tty [empty directory]
TEST_F(DeviceInfoTechnologyTest, CDCEtherModem1) {
  FilePath device_root(temp_dir_.path().Append("sys/devices/virtual/0"));
  FilePath device_path(device_root.Append("00"));
  EXPECT_TRUE(file_util::CreateDirectory(device_path));
  CreateInfoSymLink("device", device_path.value());
  EXPECT_TRUE(file_util::CreateSymbolicLink(FilePath("/drivers/cdc_ether"),
                                            device_path.Append("driver")));
  EXPECT_TRUE(file_util::CreateDirectory(device_root.Append("01/tty")));
  EXPECT_EQ(Technology::kCellular, GetDeviceTechnology());
}

// Modem with relative driver path with top-level tty file.
//   /sys/class/net/dev0/device -> ../../../device_dir/0/00
//   /sys/device_dir/0/00/driver -> /drivers/cdc_ether
//   /sys/device_dir/0/01/tty [empty directory]
TEST_F(DeviceInfoTechnologyTest, CDCEtherModem2) {
  CreateInfoSymLink("device", "../../../device_dir/0/00");
  FilePath device_root(temp_dir_.path().Append("sys/device_dir/0"));
  FilePath device_path(device_root.Append("00"));
  EXPECT_TRUE(file_util::CreateDirectory(device_path));
  EXPECT_TRUE(file_util::CreateSymbolicLink(FilePath("/drivers/cdc_ether"),
                                            device_path.Append("driver")));
  EXPECT_TRUE(file_util::CreateDirectory(device_root.Append("01/tty")));
  EXPECT_EQ(Technology::kCellular, GetDeviceTechnology());
}

// Modem with relative driver path with lower-level tty file.
//   /sys/class/net/dev0/device -> ../../../device_dir/0/00
//   /sys/device_dir/0/00/driver -> /drivers/cdc_ether
//   /sys/device_dir/0/01/yyy/tty [empty directory]
TEST_F(DeviceInfoTechnologyTest, CDCEtherModem3) {
  CreateInfoSymLink("device", "../../../device_dir/0/00");
  FilePath device_root(temp_dir_.path().Append("sys/device_dir/0"));
  FilePath device_path(device_root.Append("00"));
  EXPECT_TRUE(file_util::CreateDirectory(device_path));
  EXPECT_TRUE(file_util::CreateSymbolicLink(FilePath("/drivers/cdc_ether"),
                                            device_path.Append("driver")));
  EXPECT_TRUE(file_util::CreateDirectory(device_root.Append("01/yyy/tty")));
  EXPECT_EQ(Technology::kCellular, GetDeviceTechnology());
}

TEST_F(DeviceInfoTechnologyTest, CDCEtherNonModem) {
  CreateInfoSymLink("device", "device_dir");
  CreateInfoSymLink("device_dir/driver", "cdc_ether");
  EXPECT_EQ(Technology::kEthernet, GetDeviceTechnology());
}

TEST_F(DeviceInfoTechnologyTest, PseudoModem) {
  SetDeviceName("pseudomodem");
  CreateInfoSymLink("device", "device_dir");
  CreateInfoSymLink("device_dir/driver", "cdc_ether");
  EXPECT_EQ(Technology::kCellular, GetDeviceTechnology());

  SetDeviceName("pseudomodem9");
  CreateInfoSymLink("device", "device_dir");
  CreateInfoSymLink("device_dir/driver", "cdc_ether");
  EXPECT_EQ(Technology::kCellular, GetDeviceTechnology());
}

}  // namespace shill
