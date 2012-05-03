// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/device_info.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/if_tun.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/ether.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <string>

#include <base/bind.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/stl_util.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <base/stringprintf.h>

#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_stub.h"
#include "shill/ethernet.h"
#include "shill/manager.h"
#include "shill/routing_table.h"
#include "shill/rtnl_handler.h"
#include "shill/rtnl_listener.h"
#include "shill/rtnl_message.h"
#include "shill/scope_logger.h"
#include "shill/service.h"
#include "shill/virtio_ethernet.h"
#include "shill/wifi.h"

using base::Bind;
using base::StringPrintf;
using base::Unretained;
using std::map;
using std::string;
using std::vector;

namespace shill {

// static
const char DeviceInfo::kModemPseudoDeviceNamePrefix[] = "pseudomodem";
const char DeviceInfo::kDeviceInfoRoot[] = "/sys/class/net";
const char DeviceInfo::kDriverVirtioNet[] = "virtio_net";
const char DeviceInfo::kInterfaceUevent[] = "uevent";
const char DeviceInfo::kInterfaceUeventWifiSignature[] = "DEVTYPE=wlan\n";
const char DeviceInfo::kInterfaceDevice[] = "device";
const char DeviceInfo::kInterfaceDriver[] = "device/driver";
const char DeviceInfo::kInterfaceTunFlags[] = "tun_flags";
const char DeviceInfo::kInterfaceType[] = "type";
const char DeviceInfo::kDriverCdcEther[] = "cdc_ether";
const char *DeviceInfo::kModemDrivers[] = {
    "gobi",
    "QCUSBNet2k",
    "GobiNet"
};
const char DeviceInfo::kTunDeviceName[] = "/dev/net/tun";

DeviceInfo::DeviceInfo(ControlInterface *control_interface,
                       EventDispatcher *dispatcher,
                       Metrics *metrics,
                       Manager *manager)
    : control_interface_(control_interface),
      dispatcher_(dispatcher),
      metrics_(metrics),
      manager_(manager),
      link_callback_(Bind(&DeviceInfo::LinkMsgHandler, Unretained(this))),
      address_callback_(Bind(&DeviceInfo::AddressMsgHandler, Unretained(this))),
      link_listener_(NULL),
      address_listener_(NULL),
      device_info_root_(kDeviceInfoRoot),
      routing_table_(RoutingTable::GetInstance()),
      rtnl_handler_(RTNLHandler::GetInstance()) {
}

DeviceInfo::~DeviceInfo() {}

void DeviceInfo::AddDeviceToBlackList(const string &device_name) {
  black_list_.insert(device_name);
}

bool DeviceInfo::IsDeviceBlackListed(const string &device_name) {
  return ContainsKey(black_list_, device_name);
}

void DeviceInfo::Start() {
  link_listener_.reset(
      new RTNLListener(RTNLHandler::kRequestLink, link_callback_));
  address_listener_.reset(
      new RTNLListener(RTNLHandler::kRequestAddr, address_callback_));
  rtnl_handler_->RequestDump(RTNLHandler::kRequestLink |
                             RTNLHandler::kRequestAddr);
}

void DeviceInfo::Stop() {
  link_listener_.reset();
  address_listener_.reset();
  infos_.clear();
}

void DeviceInfo::RegisterDevice(const DeviceRefPtr &device) {
  SLOG(Device, 2) << __func__ << "(" << device->link_name() << ", "
                  << device->interface_index() << ")";
  CHECK(!GetDevice(device->interface_index()).get());
  infos_[device->interface_index()].device = device;
  if (device->TechnologyIs(Technology::kCellular) ||
      device->TechnologyIs(Technology::kEthernet) ||
      device->TechnologyIs(Technology::kWifi)) {
    manager_->RegisterDevice(device);
  }
}

void DeviceInfo::DeregisterDevice(const DeviceRefPtr &device) {
  int interface_index = device->interface_index();

  SLOG(Device, 2) << __func__ << "(" << device->link_name() << ", "
                  << interface_index << ")";
  CHECK(device->TechnologyIs(Technology::kCellular));

  // Release reference to the device
  map<int, Info>::iterator iter = infos_.find(interface_index);
  if (iter != infos_.end()) {
    SLOG(Device, 2) << "Removing device from info for index: "
                    << interface_index;
    manager_->DeregisterDevice(device);
    // Release the reference to the device, but maintain the mapping
    // for the index.  That will be cleaned up by an RTNL message.
    iter->second.device = NULL;
  }
}

FilePath DeviceInfo::GetDeviceInfoPath(const string &iface_name,
                                       const string &path_name) {
  return device_info_root_.Append(iface_name).Append(path_name);
}

bool DeviceInfo::GetDeviceInfoContents(const string &iface_name,
                                       const string &path_name,
                                       string *contents_out) {
  return file_util::ReadFileToString(GetDeviceInfoPath(iface_name, path_name),
                                     contents_out);
}
bool DeviceInfo::GetDeviceInfoSymbolicLink(const string &iface_name,
                                      const string &path_name,
                                      FilePath *path_out) {
  return file_util::ReadSymbolicLink(GetDeviceInfoPath(iface_name, path_name),
                                     path_out);
}

Technology::Identifier DeviceInfo::GetDeviceTechnology(
    const string &iface_name) {
  string type_string;
  int arp_type = ARPHRD_VOID;;
  if (GetDeviceInfoContents(iface_name, kInterfaceType, &type_string) &&
      TrimString(type_string, "\n", &type_string) &&
      !base::StringToInt(type_string, &arp_type)) {
    arp_type = ARPHRD_VOID;
  }

  string contents;
  if (!GetDeviceInfoContents(iface_name, kInterfaceUevent, &contents)) {
    SLOG(Device, 2) << StringPrintf("%s: device %s has no uevent file",
                                    __func__, iface_name.c_str());
    return Technology::kUnknown;
  }

  // If the "uevent" file contains the string "DEVTYPE=wlan\n" at the
  // start of the file or after a newline, we can safely assume this
  // is a wifi device.
  if (contents.find(kInterfaceUeventWifiSignature) != string::npos) {
    SLOG(Device, 2)
        << StringPrintf("%s: device %s has wifi signature in uevent file",
                        __func__, iface_name.c_str());
    if (arp_type == ARPHRD_IEEE80211_RADIOTAP) {
      SLOG(Device, 2) << StringPrintf("%s: wifi device %s is in monitor mode",
                                      __func__, iface_name.c_str());
      return Technology::kWiFiMonitor;
    }
    return Technology::kWifi;
  }

  FilePath driver_path;
  if (!GetDeviceInfoSymbolicLink(iface_name, kInterfaceDriver, &driver_path)) {
    SLOG(Device, 2) << StringPrintf("%s: device %s has no device symlink",
                                    __func__, iface_name.c_str());
    if (arp_type == ARPHRD_LOOPBACK) {
      SLOG(Device, 2) << StringPrintf("%s: device %s is a loopback device",
                                      __func__, iface_name.c_str());
      return Technology::kLoopback;
    }
    if (arp_type == ARPHRD_PPP) {
      SLOG(Device, 2) << StringPrintf("%s: device %s is a ppp device",
                                      __func__, iface_name.c_str());
      return Technology::kPPP;
    }
    string tun_flags_str;
    int tun_flags = 0;
    if (GetDeviceInfoContents(iface_name, kInterfaceTunFlags, &tun_flags_str) &&
        TrimString(tun_flags_str, "\n", &tun_flags_str) &&
        base::HexStringToInt(tun_flags_str, &tun_flags) &&
        (tun_flags & IFF_TUN)) {
      SLOG(Device, 2) << StringPrintf("%s: device %s is tun device",
                                      __func__, iface_name.c_str());
      return Technology::kTunnel;
    }
    return Technology::kUnknown;
  }

  string driver_name(driver_path.BaseName().value());
  // See if driver for this interface is in a list of known modem driver names.
  for (size_t modem_idx = 0; modem_idx < arraysize(kModemDrivers);
       ++modem_idx) {
    if (driver_name == kModemDrivers[modem_idx]) {
      SLOG(Device, 2)
          << StringPrintf("%s: device %s is matched with modem driver %s",
                          __func__, iface_name.c_str(), driver_name.c_str());
      return Technology::kCellular;
    }
  }

  // For cdc_ether devices, make sure it's a modem because this driver
  // can be used for other ethernet devices.
  if (driver_name == kDriverCdcEther &&
      IsCdcEtherModemDevice(iface_name)) {
    SLOG(Device, 2) << StringPrintf("%s: device %s is a modem cdc_ether device",
                                    __func__, iface_name.c_str());
    return Technology::kCellular;
  }

  // Special case for pseudo modems which are used for testing
  if (iface_name.find(kModemPseudoDeviceNamePrefix) == 0) {
    SLOG(Device, 2) << StringPrintf(
        "%s: device %s is a pseudo modem for testing",
        __func__, iface_name.c_str());
    return Technology::kCellular;
  }

  // Special case for the virtio driver, used when run under KVM. See also
  // the comment in VirtioEthernet::Start.
  if (driver_name == kDriverVirtioNet) {
    SLOG(Device, 2) << StringPrintf("%s: device %s is virtio ethernet",
                                    __func__, iface_name.c_str());
    return Technology::kVirtioEthernet;
  }

  SLOG(Device, 2) << StringPrintf("%s: device %s, with driver %s, "
                                  "is defaulted to type ethernet",
                                  __func__, iface_name.c_str(),
                                  driver_name.c_str());
  return Technology::kEthernet;
}

bool DeviceInfo::IsCdcEtherModemDevice(const std::string &iface_name) {
  // A cdc_ether device is a modem device if it also exposes tty interfaces.
  // To determine this, we look for the existence of the tty interface in the
  // USB device sysfs tree.
  //
  // A typical sysfs dir hierarchy for a cdc_ether modem USB device is as
  // follows:
  //
  //   /sys/devices/pci0000:00/0000:00:1d.7/usb1/1-2
  //     1-2:1.0
  //       tty
  //         ttyACM0
  //     1-2:1.1
  //       net
  //         usb0
  //     1-2:1.2
  //       tty
  //         ttyACM1
  //       ...
  //
  // /sys/class/net/usb0/device symlinks to
  // /sys/devices/pci0000:00/0000:00:1d.7/usb1/1-2/1-2:1.1
  //
  // Note that some modem devices have the tty directory one level deeper
  // (eg. E362), so the device tree for the tty interface is:
  // /sys/devices/pci0000:00/0000:00:1d.7/usb/1-2/1-2:1.0/ttyUSB0/tty/ttyUSB0

  FilePath device_file = GetDeviceInfoPath(iface_name, kInterfaceDevice);
  FilePath device_path;
  if (!file_util::ReadSymbolicLink(device_file, &device_path)) {
    SLOG(Device, 2) << StringPrintf("%s: device %s has no device symlink",
                                    __func__, iface_name.c_str());
    return false;
  }
  if (!device_path.IsAbsolute()) {
    device_path = device_file.DirName().Append(device_path);
    file_util::AbsolutePath(&device_path);
  }

  // Look for tty interface by enumerating all directories under the parent
  // USB device and see if there's a subdirectory "tty" inside.  In other
  // words, using the example dir hierarchy above, find
  // /sys/devices/pci0000:00/0000:00:1d.7/usb1/1-2/.../tty.
  // If this exists, then this is a modem device.
  return HasSubdir(device_path.DirName(), FilePath("tty"));
}

// static
bool DeviceInfo::HasSubdir(const FilePath &base_dir, const FilePath &subdir) {
  file_util::FileEnumerator::FileType type =
      static_cast<file_util::FileEnumerator::FileType>(
          file_util::FileEnumerator::DIRECTORIES |
          file_util::FileEnumerator::SHOW_SYM_LINKS);
  file_util::FileEnumerator dir_enum(base_dir, true, type);
  for (FilePath curr_dir = dir_enum.Next(); !curr_dir.empty();
       curr_dir = dir_enum.Next()) {
    if (curr_dir.BaseName() == subdir)
      return true;
  }
  return false;
}

DeviceRefPtr DeviceInfo::CreateDevice(const string &link_name,
                                      const string &address,
                                      int interface_index,
                                      Technology::Identifier technology) {
  DeviceRefPtr device;

  switch (technology) {
    case Technology::kCellular:
      // Cellular devices are managed by ModemInfo.
      SLOG(Device, 2) << "Cellular link " << link_name
                      << " at index " << interface_index
                      << " -- notifying ModemInfo.";
      manager_->modem_info()->OnDeviceInfoAvailable(link_name);
      break;
    case Technology::kEthernet:
      device = new Ethernet(control_interface_, dispatcher_, metrics_,
                            manager_, link_name, address, interface_index);
      device->EnableIPv6Privacy();
      break;
    case Technology::kVirtioEthernet:
      device = new VirtioEthernet(control_interface_, dispatcher_, metrics_,
                                  manager_, link_name, address,
                                  interface_index);
      device->EnableIPv6Privacy();
      break;
    case Technology::kWifi:
      device = new WiFi(control_interface_, dispatcher_, metrics_, manager_,
                        link_name, address, interface_index);
      device->EnableIPv6Privacy();
      break;
    case Technology::kPPP:
    case Technology::kTunnel:
      // Tunnel and PPP devices are managed by the VPN code (PPP for
      // l2tpipsec).  Notify the VPN Provider of the interface's presence.
      // Since CreateDevice is only called once in the lifetime of an
      // interface index, this notification will only occur the first
      // time the device is seen.
      SLOG(Device, 2) << "Tunnel / PPP link " << link_name
                      << " at index " << interface_index
                      << " -- notifying VPNProvider.";
      if (!manager_->vpn_provider()->OnDeviceInfoAvailable(link_name,
                                                           interface_index) &&
          technology == Technology::kTunnel) {
        // If VPN does not know anything about this tunnel, it is probably
        // left over from a previous instance and should not exist.
        SLOG(Device, 2) << "Tunnel link is unused.  Deleting.";
        DeleteInterface(interface_index);
      }
      break;
    case Technology::kLoopback:
      // Loopback devices are largely ignored, but we should make sure the
      // link is enabled.
      SLOG(Device, 2) << "Bringing up loopback device " << link_name
                      << " at index " << interface_index;
      rtnl_handler_->SetInterfaceFlags(interface_index, IFF_UP, IFF_UP);
      return NULL;
    default:
      // We will not manage this device in shill.  Do not create a device
      // object or do anything to change its state.  We create a stub object
      // which is useful for testing.
      return new DeviceStub(control_interface_, dispatcher_, metrics_,
                            manager_, link_name, address, interface_index,
                            technology);;
  }

  // Reset the routing table and addresses.
  routing_table_->FlushRoutes(interface_index);
  FlushAddresses(interface_index);

  return device;
}

void DeviceInfo::AddLinkMsgHandler(const RTNLMessage &msg) {
  DCHECK(msg.type() == RTNLMessage::kTypeLink &&
         msg.mode() == RTNLMessage::kModeAdd);
  int dev_index = msg.interface_index();
  Technology::Identifier technology = Technology::kUnknown;

  unsigned int flags = msg.link_status().flags;
  unsigned int change = msg.link_status().change;
  bool new_device =
      !ContainsKey(infos_, dev_index) || infos_[dev_index].has_addresses_only;
  SLOG(Device, 2) << __func__ << "(index=" << dev_index
                  << std::showbase << std::hex
                  << ", flags=" << flags << ", change=" << change << ")"
                  << std::dec << std::noshowbase
                  << ", new_device=" << new_device;
  infos_[dev_index].has_addresses_only = false;
  infos_[dev_index].flags = flags;

  DeviceRefPtr device = GetDevice(dev_index);
  if (new_device) {
    CHECK(!device);
    if (!msg.HasAttribute(IFLA_IFNAME)) {
      LOG(ERROR) << "Add Link message does not have IFLA_IFNAME!";
      return;
    }
    ByteString b(msg.GetAttribute(IFLA_IFNAME));
    string link_name(reinterpret_cast<const char*>(b.GetConstData()));
    SLOG(Device, 2) << "add link index "  << dev_index << " name " << link_name;
    infos_[dev_index].name = link_name;
    indices_[link_name] = dev_index;

    if (!link_name.empty()) {
      if (ContainsKey(black_list_, link_name)) {
        technology = Technology::kBlacklisted;
      } else {
        technology = GetDeviceTechnology(link_name);
      }
    }
    string address;
    if (msg.HasAttribute(IFLA_ADDRESS)) {
      infos_[dev_index].mac_address = msg.GetAttribute(IFLA_ADDRESS);
      address = StringToLowerASCII(infos_[dev_index].mac_address.HexEncode());
      SLOG(Device, 2) << "link index " << dev_index << " address "
                      << infos_[dev_index].mac_address.HexEncode();
    } else if (technology != Technology::kTunnel &&
               technology != Technology::kPPP) {
      LOG(ERROR) << "Add Link message does not have IFLA_ADDRESS!";
      return;
    }
    device = CreateDevice(link_name, address, dev_index, technology);
    if (device) {
      RegisterDevice(device);
    }
  }
  if (device) {
    device->LinkEvent(flags, change);
  }
}

void DeviceInfo::DelLinkMsgHandler(const RTNLMessage &msg) {
  SLOG(Device, 2) << __func__ << "(index=" << msg.interface_index() << ")";

  DCHECK(msg.type() == RTNLMessage::kTypeLink &&
         msg.mode() == RTNLMessage::kModeDelete);
  SLOG(Device, 2) << __func__ << "(index=" << msg.interface_index()
                  << std::showbase << std::hex
                  << ", flags=" << msg.link_status().flags
                  << ", change=" << msg.link_status().change << ")";
  RemoveInfo(msg.interface_index());
}

DeviceRefPtr DeviceInfo::GetDevice(int interface_index) const {
  const Info *info = GetInfo(interface_index);
  return info ? info->device : NULL;
}

int DeviceInfo::GetIndex(const string &interface_name) const {
  map<string, int>::const_iterator it = indices_.find(interface_name);
  return it == indices_.end() ? -1 : it->second;
}

bool DeviceInfo::GetMACAddress(int interface_index, ByteString *address) const {
  const Info *info = GetInfo(interface_index);
  if (!info) {
    return false;
  }
  *address = info->mac_address;
  return true;
}

bool DeviceInfo::GetAddresses(int interface_index,
                              vector<AddressData> *addresses) const {
  const Info *info = GetInfo(interface_index);
  if (!info) {
    return false;
  }
  *addresses = info->ip_addresses;
  return true;
}

void DeviceInfo::FlushAddresses(int interface_index) const {
  SLOG(Device, 2) << __func__ << "(" << interface_index << ")";
  const Info *info = GetInfo(interface_index);
  if (!info) {
    return;
  }
  const vector<AddressData> &addresses = info->ip_addresses;
  vector<AddressData>::const_iterator iter;
  for (iter = addresses.begin(); iter != addresses.end(); ++iter) {
    if (iter->address.family() == IPAddress::kFamilyIPv4 ||
        (iter->scope == RT_SCOPE_UNIVERSE &&
         (iter->flags & ~IFA_F_TEMPORARY) == 0)) {
      SLOG(Device, 2) << __func__ << ": removing ip address "
                      << iter->address.ToString()
                      << " from interface " << interface_index;
      rtnl_handler_->RemoveInterfaceAddress(interface_index, iter->address);
    }
  }
}

bool DeviceInfo::GetFlags(int interface_index, unsigned int *flags) const {
  const Info *info = GetInfo(interface_index);
  if (!info) {
    return false;
  }
  *flags = info->flags;
  return true;
}

bool DeviceInfo::CreateTunnelInterface(string *interface_name) const {
  int fd = HANDLE_EINTR(open(kTunDeviceName, O_RDWR));
  if (fd < 0) {
    PLOG(ERROR) << "failed to open " << kTunDeviceName;
    return false;
  }
  file_util::ScopedFD scoped_fd(&fd);

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
  if (HANDLE_EINTR(ioctl(fd, TUNSETIFF, &ifr))) {
    PLOG(ERROR) << "failed to create tunnel interface";
    return false;
  }

  if (HANDLE_EINTR(ioctl(fd, TUNSETPERSIST, 1))) {
    PLOG(ERROR) << "failed to set tunnel interface to be persistent";
    return false;
  }

  *interface_name = string(ifr.ifr_name);

  return true;
}

bool DeviceInfo::DeleteInterface(int interface_index) const {
  return rtnl_handler_->RemoveInterface(interface_index);
}

const DeviceInfo::Info *DeviceInfo::GetInfo(int interface_index) const {
  map<int, Info>::const_iterator iter = infos_.find(interface_index);
  if (iter == infos_.end()) {
    return NULL;
  }
  return &iter->second;
}

void DeviceInfo::RemoveInfo(int interface_index) {
  map<int, Info>::iterator iter = infos_.find(interface_index);
  if (iter != infos_.end()) {
    SLOG(Device, 2) << "Removing info for device index: " << interface_index;
    if (iter->second.device.get()) {
      manager_->DeregisterDevice(iter->second.device);
    }
    indices_.erase(iter->second.name);
    infos_.erase(iter);
  } else {
    SLOG(Device, 2) << __func__ << ": Unknown device index: "
                    << interface_index;
  }
}

void DeviceInfo::LinkMsgHandler(const RTNLMessage &msg) {
  DCHECK(msg.type() == RTNLMessage::kTypeLink);
  if (msg.mode() == RTNLMessage::kModeAdd) {
    AddLinkMsgHandler(msg);
  } else if (msg.mode() == RTNLMessage::kModeDelete) {
    DelLinkMsgHandler(msg);
  } else {
    NOTREACHED();
  }
}

void DeviceInfo::AddressMsgHandler(const RTNLMessage &msg) {
  SLOG(Device, 2) << __func__;
  DCHECK(msg.type() == RTNLMessage::kTypeAddress);
  int interface_index = msg.interface_index();
  if (!ContainsKey(infos_, interface_index)) {
    SLOG(Device, 2) << "Got advance address information for unknown index "
                    << interface_index;
    infos_[interface_index].has_addresses_only = true;
  }
  const RTNLMessage::AddressStatus &status = msg.address_status();
  IPAddress address(msg.family(),
                    msg.GetAttribute(IFA_ADDRESS),
                    status.prefix_len);

  vector<AddressData> &address_list = infos_[interface_index].ip_addresses;
  vector<AddressData>::iterator iter;
  for (iter = address_list.begin(); iter != address_list.end(); ++iter) {
    if (address.Equals(iter->address)) {
      break;
    }
  }
  if (iter != address_list.end()) {
    if (msg.mode() == RTNLMessage::kModeDelete) {
      SLOG(Device, 2) << "Delete address for interface " << interface_index;
      address_list.erase(iter);
    } else {
      iter->flags = status.flags;
      iter->scope = status.scope;
    }
  } else if (msg.mode() == RTNLMessage::kModeAdd) {
    address_list.push_back(AddressData(address, status.flags, status.scope));
    SLOG(Device, 2) << "Add address " << address.ToString()
                    << " for interface " << interface_index;
  }
}

}  // namespace shill
