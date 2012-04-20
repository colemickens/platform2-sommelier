// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem.h"

#include <base/file_util.h>
#include <mm/ModemManager-enums.h>
#include <mm/ModemManager-names.h>

#include "shill/cellular.h"
#include "shill/device_info.h"

using std::string;

namespace shill {

namespace {
// The default place where the system keeps symbolic links for network device
const char kDefaultNetfilesPath[] = "/sys/class/net";
} // namespace {}

Modem1::Modem1(const string &owner,
               const string &path,
               ControlInterface *control_interface,
               EventDispatcher *dispatcher,
               Metrics *metrics,
               Manager *manager,
               mobile_provider_db *provider_db)
    : Modem(owner, path, control_interface, dispatcher, metrics, manager,
            provider_db),
      netfiles_path_(kDefaultNetfilesPath) {
}

Modem1::~Modem1() {}

bool Modem1::GetLinkName(const DBusPropertiesMap &modem_props,
                         string *name) const {
  string device_prop;
  if (!DBusProperties::GetString(modem_props,
                                 MM_MODEM_PROPERTY_DEVICE,
                                 &device_prop)) {
    LOG(ERROR) << "Device missing property: " << MM_MODEM_PROPERTY_DEVICE;
    return false;
  }

  if (device_prop.find(DeviceInfo::kModemPseudoDeviceNamePrefix) == 0) {
    *name = device_prop;
    return true;
  }

  // |device_prop| will be a sysfs path such as:
  //  /sys/devices/pci0000:00/0000:00:1d.7/usb1/1-2
  FilePath device_path(device_prop);

  // Each entry in |netfiles_path_" (typically /sys/class/net)
  // has the name of a network interface and is a symlink into the
  // actual device structure:
  //  eth0 -> ../../devices/pci0000:00/0000:00:1c.5/0000:01:00.0/net/eth0
  // Iterate over all of these and see if any of them point into
  // subdirectories of the sysfs path from the Device property.
  // FileEnumerator warns that it is a blocking interface; that
  // shouldn't be a problem here.
  file_util::FileEnumerator netfiles(netfiles_path_,
                                     false, // don't recurse
                                     file_util::FileEnumerator::DIRECTORIES);
  for (FilePath link = netfiles.Next(); !link.empty(); link = netfiles.Next()) {
    FilePath target;
    if (!file_util::ReadSymbolicLink(link, &target))
      continue;
    if (!target.IsAbsolute())
      target = netfiles_path_.Append(target);
    if (file_util::ContainsPath(device_path, target)) {
      *name = link.BaseName().value();
      return true;
    }
  }
  LOG(ERROR) << "No link name found for: " << device_prop;
  return false;
}

void Modem1::CreateDeviceMM1(const DBusInterfaceToProperties &i_to_p) {
  Init();
  DBusInterfaceToProperties::const_iterator modem_properties =
      i_to_p.find(MM_DBUS_INTERFACE_MODEM);
  if (modem_properties == i_to_p.end()) {
    LOG(ERROR) << "Cellular device with no modem properties";
    return;
  }
  set_type(Cellular::kTypeUniversal);

  // We cannot check the IP method to make sure it's not PPP. The IP
  // method will be checked later when the bearer object is fetched.
  // TODO(jglasgow): We should pass i_to_p because there are lots of
  // properties we might want
  CreateDeviceFromModemProperties(modem_properties->second);
}

string Modem1::GetModemInterface(void) const {
  return string(MM_DBUS_INTERFACE_MODEM);
}

}  // namespace shill
