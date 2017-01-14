//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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

class Throttler;
using ThrottlerRefPtr = scoped_refptr<Throttler>;

class Profile;
using ProfileConstRefPtr = scoped_refptr<const Profile>;
using ProfileRefPtr = scoped_refptr<Profile>;

class Connection;
using ConnectionRefPtr = scoped_refptr<Connection>;

}  // namespace shill

#endif  // SHILL_REFPTR_TYPES_H_
