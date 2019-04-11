// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_REFPTR_TYPES_H_
#define SHILL_REFPTR_TYPES_H_

#include <base/memory/ref_counted.h>

namespace shill {

class Device;
using DeviceConstRefPtr = scoped_refptr<const Device>;
using DeviceRefPtr = scoped_refptr<Device>;

class Cellular;
using CellularConstRefPtr = scoped_refptr<const Cellular>;
using CellularRefPtr = scoped_refptr<Cellular>;

class Ethernet;
using EthernetConstRefPtr = scoped_refptr<const Ethernet>;
using EthernetRefPtr = scoped_refptr<Ethernet>;

class PPPDevice;
using PPPDeviceConstRefPtr = scoped_refptr<const PPPDevice>;
using PPPDeviceRefPtr = scoped_refptr<PPPDevice>;

class VirtualDevice;
using VirtualDeviceConstRefPtr = scoped_refptr<const VirtualDevice>;
using VirtualDeviceRefPtr = scoped_refptr<VirtualDevice>;

class WiFi;
using WiFiConstRefPtr = scoped_refptr<const WiFi>;
using WiFiRefPtr = scoped_refptr<WiFi>;

class WiMax;
using WiMaxConstRefPtr = scoped_refptr<const WiMax>;
using WiMaxRefPtr = scoped_refptr<WiMax>;

class WiFiEndpoint;
using WiFiEndpointConstRefPtr = scoped_refptr<const WiFiEndpoint>;
using WiFiEndpointRefPtr = scoped_refptr<WiFiEndpoint>;

class Service;
using ServiceConstRefPtr = scoped_refptr<const Service>;
using ServiceRefPtr = scoped_refptr<Service>;

class CellularService;
using CellularServiceConstRefPtr = scoped_refptr<const CellularService>;
using CellularServiceRefPtr = scoped_refptr<CellularService>;

class EthernetService;
using EthernetServiceConstRefPtr = scoped_refptr<const EthernetService>;
using EthernetServiceRefPtr = scoped_refptr<EthernetService>;

class VPNService;
using VPNServiceConstRefPtr = scoped_refptr<const VPNService>;
using VPNServiceRefPtr = scoped_refptr<VPNService>;

class WiFiService;
using WiFiServiceConstRefPtr = scoped_refptr<const WiFiService>;
using WiFiServiceRefPtr = scoped_refptr<WiFiService>;

class WiMaxService;
using WiMaxServiceConstRefPtr = scoped_refptr<const WiMaxService>;
using WiMaxServiceRefPtr = scoped_refptr<WiMaxService>;

class IPConfig;
using IPConfigRefPtr = scoped_refptr<IPConfig>;

class DHCPConfig;
using DHCPConfigRefPtr = scoped_refptr<DHCPConfig>;

class Profile;
using ProfileConstRefPtr = scoped_refptr<const Profile>;
using ProfileRefPtr = scoped_refptr<Profile>;

class Connection;
using ConnectionConstRefPtr = scoped_refptr<const Connection>;
using ConnectionRefPtr = scoped_refptr<Connection>;

}  // namespace shill

#endif  // SHILL_REFPTR_TYPES_H_
