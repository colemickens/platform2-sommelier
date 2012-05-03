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
#include <base/file_path.h>
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
class RoutingTable;
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

  // Device name prefix for modem pseudo devices used in testing.
  static const char kModemPseudoDeviceNamePrefix[];

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
  virtual void RegisterDevice(const DeviceRefPtr &device);

  // Remove |device| from this DeviceInfo.  This function should only
  // be called for cellular devices because the lifetime of the
  // cellular devices is controlled by the Modem object and its
  // communication to modem manager, rather than by RTNL messages.
  virtual void DeregisterDevice(const DeviceRefPtr &device);

  virtual DeviceRefPtr GetDevice(int interface_index) const;
  virtual bool GetMACAddress(int interface_index, ByteString *address) const;
  virtual bool GetFlags(int interface_index, unsigned int *flags) const;
  virtual bool GetAddresses(int interface_index,
                            std::vector<AddressData> *addresses) const;
  virtual void FlushAddresses(int interface_index) const;
  virtual bool CreateTunnelInterface(std::string *interface_name) const;
  virtual bool DeleteInterface(int interface_index) const;

  // Returns the interface index for |interface_name| or -1 if unknown.
  virtual int GetIndex(const std::string &interface_name) const;

 private:
  friend class DeviceInfoTechnologyTest;
  friend class DeviceInfoTest;
  FRIEND_TEST(CellularTest, StartLinked);
  FRIEND_TEST(DeviceInfoTest, HasSubdir);  // For HasSubdir.
  FRIEND_TEST(DeviceInfoTest, StartStop);

  struct Info {
    Info() : flags(0), has_addresses_only(false) {}

    DeviceRefPtr device;
    std::string name;
    ByteString mac_address;
    std::vector<AddressData> ip_addresses;
    unsigned int flags;
    // This flag indicates that link information has not been retrieved yet;
    // only the ip_addresses field is valid.
    bool has_addresses_only;
  };

  // Root of the kernel sysfs directory holding network device info.
  static const char kDeviceInfoRoot[];
  // Name of the virtio network driver.
  static const char kDriverVirtioNet[];
  // Sysfs path to a device uevent file.
  static const char kInterfaceUevent[];
  // Content of a device uevent file that indicates it is a wifi device.
  static const char kInterfaceUeventWifiSignature[];
  // Sysfs path to a device via its interface name.
  static const char kInterfaceDevice[];
  // Sysfs path to the driver of a device via its interface name.
  static const char kInterfaceDriver[];
  // Sysfs path to the file that is used to determine if this is tun device.
  static const char kInterfaceTunFlags[];
  // Sysfs path to the file that is used to determine if a wifi device is
  // operating in monitor mode.
  static const char kInterfaceType[];
  // Name of the "cdc_ether" driver.  This driver is not included in the
  // kModemDrivers list because we need to do additional checking.
  static const char kDriverCdcEther[];
  // Modem drivers that we support.
  static const char *kModemDrivers[];
  // Path to the tun device.
  static const char kTunDeviceName[];

  // Create a Device object for the interface named |linkname|, with a
  // string-form MAC address |address|, whose kernel interface index
  // is |interface_index| and detected technology is |technology|.
  DeviceRefPtr CreateDevice(const std::string &link_name,
                            const std::string &address,
                            int interface_index,
                            Technology::Identifier technology);

  // Return the FilePath for a given |path_name| in the device sysinfo for
  // a specific interface |iface_name|.
  FilePath GetDeviceInfoPath(const std::string &iface_name,
                             const std::string &path_name);
  // Return the contents of the device info file |path_name| for interface
  // |iface_name| in output parameter |contents_out|.  Returns true if file
  // read succeeded, false otherwise.
  bool GetDeviceInfoContents(const std::string &iface_name,
                             const std::string &path_name,
                             std::string *contents_out);

  // Return the filepath for the target of the device info symbolic link
  // |path_name| for interface |iface_name| in output parameter |path_out|.
  // Returns true if symbolic link read succeeded, false otherwise.
  bool GetDeviceInfoSymbolicLink(const std::string &iface_name,
                                 const std::string &path_name,
                                 FilePath *path_out);
  // Classify the device named |iface_name|, and return an identifier
  // indicating its type.
  Technology::Identifier GetDeviceTechnology(const std::string &iface_name);
  // Checks the device specified by |iface_name| to see if it's a modem device.
  // This method assumes that |iface_name| has already been determined to be
  // using the cdc_ether driver.
  bool IsCdcEtherModemDevice(const std::string &iface_name);
  // Returns true if |base_dir| has a subdirectory named |subdir|.
  // |subdir| can be an immediate subdirectory of |base_dir| or can be
  // several levels deep.
  static bool HasSubdir(const FilePath &base_dir, const FilePath &subdir);

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

  std::map<int, Info> infos_;  // Maps interface index to Info.
  std::map<std::string, int> indices_;  // Maps interface name to index.

  base::Callback<void(const RTNLMessage &)> link_callback_;
  base::Callback<void(const RTNLMessage &)> address_callback_;
  scoped_ptr<RTNLListener> link_listener_;
  scoped_ptr<RTNLListener> address_listener_;
  std::set<std::string> black_list_;
  FilePath device_info_root_;

  // Cache copy of singleton pointers.
  RoutingTable *routing_table_;
  RTNLHandler *rtnl_handler_;

  DISALLOW_COPY_AND_ASSIGN(DeviceInfo);
};

}  // namespace shill

#endif  // SHILL_DEVICE_INFO_
