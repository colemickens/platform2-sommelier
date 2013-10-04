// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ppp_device.h"

#include <base/stl_util.h>

#include "shill/logging.h"
#include "shill/technology.h"

using std::map;
using std::string;

namespace shill {

// statics
const char PPPDevice::kDaemonPath[] = "/usr/sbin/pppd";
const char PPPDevice::kPluginPath[] = SHIMDIR "/shill-pppd-plugin.so";

PPPDevice::PPPDevice(ControlInterface *control,
                     EventDispatcher *dispatcher,
                     Metrics *metrics,
                     Manager *manager,
                     const string &link_name,
                     int interface_index)
    : VirtualDevice(control, dispatcher, metrics, manager, link_name,
                    interface_index, Technology::kPPP) {}

PPPDevice::~PPPDevice() {}

void PPPDevice::UpdateIPConfigFromPPP(const map<string, string> &configuration,
                                      bool blackhole_ipv6) {
  SLOG(PPP, 2) << __func__ << " on " << link_name();
  IPConfig::Properties properties =
      ParseIPConfiguration(link_name(), configuration);
  properties.blackhole_ipv6 = blackhole_ipv6;
  UpdateIPConfig(properties);
}

// static
string PPPDevice::GetInterfaceName(const map<string, string> &configuration) {
  if (ContainsKey(configuration, kPPPInterfaceName)) {
    return configuration.find(kPPPInterfaceName)->second;
  }
  return string();
}

// static
IPConfig::Properties PPPDevice::ParseIPConfiguration(
    const string &link_name, const map<string, string> &configuration) {
  SLOG(PPP, 2) << __func__ << " on " << link_name;
  IPConfig::Properties properties;
  properties.address_family = IPAddress::kFamilyIPv4;
  properties.subnet_prefix = IPAddress::GetMaxPrefixLength(
      properties.address_family);
  for (const auto &it : configuration)  {
    const string &key = it.first;
    const string &value = it.second;
    SLOG(PPP, 2) << "Processing: " << key << " -> " << value;
    if (key == kPPPInternalIP4Address) {
      properties.address = value;
    } else if (key == kPPPExternalIP4Address) {
      properties.peer_address = value;
    } else if (key == kPPPGatewayAddress) {
      properties.gateway = value;
    } else if (key == kPPPDNS1) {
      properties.dns_servers.insert(properties.dns_servers.begin(), value);
    } else if (key == kPPPDNS2) {
      properties.dns_servers.push_back(value);
    } else if (key == kPPPLNSAddress) {
      // This is really a L2TPIPSec property. But it's sent to us by
      // our PPP plugin.
      properties.trusted_ip = value;
    } else {
      SLOG(PPP, 2) << "Key ignored.";
    }
  }
  if (properties.gateway.empty()) {
    // The gateway may be unspecified, since this is a point-to-point
    // link. Set to the peer's address, so that Connection can set the
    // routing table.
    properties.gateway = properties.peer_address;
  }
  return properties;
}

}  // namespace shill
