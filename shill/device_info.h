// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DEVICE_INFO_
#define SHILL_DEVICE_INFO_

#include <map>
#include <set>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/byte_string.h"
#include "shill/device.h"
#include "shill/ip_address.h"
#include "shill/rtnl_listener.h"
#include "shill/technology.h"

namespace shill {

class Manager;
class Metrics;
class RTNLHandler;
class RTNLMessage;

class DeviceInfo {
 public:
  struct AddressData {
    AddressData()
        : address(IPAddress::kFamilyUnknown), flags(0), scope(0) {}
    AddressData(const IPAddress &address_in,
                unsigned char flags_in,
                unsigned char scope_in)
        : address(address_in), flags(flags_in), scope(scope_in) {}
    IPAddress address;
    unsigned char flags;
    unsigned char scope;
  };

  DeviceInfo(ControlInterface *control_interface,
             EventDispatcher *dispatcher,
             Metrics *metrics,
             Manager *manager);
  ~DeviceInfo();

  void AddDeviceToBlackList(const std::string &device_name);
  bool IsDeviceBlackListed(const std::string &device_name);
  void Start();
  void Stop();

  // Adds |device| to this DeviceInfo instance so that we can handle its link
  // messages, and registers it with the manager.
  void RegisterDevice(const DeviceRefPtr &device);

  // Remove |device| from this DeviceInfo.  This function should only
  // be called for cellular devices because the lifetime of the
  // cellular devices is controlled by the Modem object and its
  // communication to modem manager, rather than by RTNL messages.
  void DeregisterDevice(const DeviceRefPtr &device);

  virtual DeviceRefPtr GetDevice(int interface_index) const;
  virtual bool GetMACAddress(int interface_index, ByteString *address) const;
  virtual bool GetFlags(int interface_index, unsigned int *flags) const;
  virtual bool GetAddresses(int interface_index,
                            std::vector<AddressData> *addresses) const;
  virtual void FlushAddresses(int interface_index) const;
  virtual bool CreateTunnelInterface(std::string *interface_name) const;
  virtual bool DeleteInterface(int interface_index) const;

 private:
  friend class DeviceInfoTest;
  FRIEND_TEST(CellularTest, StartLinked);

  struct Info {
    Info() : flags(0) {}

    DeviceRefPtr device;
    ByteString mac_address;
    std::vector<AddressData> ip_addresses;
    unsigned int flags;
  };

  static const char kInterfaceUevent[];
  static const char kInterfaceUeventWifiSignature[];
  static const char kInterfaceDriver[];
  static const char kInterfaceTunFlags[];
  static const char kInterfaceType[];
  static const char *kModemDrivers[];
  static const char kTunDeviceName[];

  static Technology::Identifier GetDeviceTechnology(
      const std::string &face_name);

  void AddLinkMsgHandler(const RTNLMessage &msg);
  void DelLinkMsgHandler(const RTNLMessage &msg);
  void LinkMsgHandler(const RTNLMessage &msg);
  void AddressMsgHandler(const RTNLMessage &msg);

  const Info *GetInfo(int interface_index) const;
  void RemoveInfo(int interface_index);
  void EnableDeviceIPv6Privacy(const std::string &link_name);

  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Metrics *metrics_;
  Manager *manager_;
  std::map<int, Info> infos_;
  base::Callback<void(const RTNLMessage &)> link_callback_;
  base::Callback<void(const RTNLMessage &)> address_callback_;
  scoped_ptr<RTNLListener> link_listener_;
  scoped_ptr<RTNLListener> address_listener_;
  std::set<std::string> black_list_;

  // Cache copy of singleton
  RTNLHandler *rtnl_handler_;

  DISALLOW_COPY_AND_ASSIGN(DeviceInfo);
};

}  // namespace shill

#endif  // SHILL_DEVICE_INFO_
