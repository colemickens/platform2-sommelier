// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcp_config.h"

#include <arpa/inet.h>

#include <base/logging.h>

#include "shill/dhcpcd_proxy.h"
#include "shill/dhcp_provider.h"
#include "shill/glib_interface.h"

using std::string;
using std::vector;

namespace shill {

const char DHCPConfig::kConfigurationKeyBroadcastAddress[] = "BroadcastAddress";
const char DHCPConfig::kConfigurationKeyDNS[] = "DomainNameServers";
const char DHCPConfig::kConfigurationKeyDomainName[] = "DomainName";
const char DHCPConfig::kConfigurationKeyDomainSearch[] = "DomainSearch";
const char DHCPConfig::kConfigurationKeyIPAddress[] = "IPAddress";
const char DHCPConfig::kConfigurationKeyMTU[] = "InterfaceMTU";
const char DHCPConfig::kConfigurationKeyRouters[] = "Routers";
const char DHCPConfig::kConfigurationKeySubnetCIDR[] = "SubnetCIDR";
const char DHCPConfig::kDHCPCDPath[] = "/sbin/dhcpcd";


DHCPConfig::DHCPConfig(DHCPProvider *provider,
                       DeviceConstRefPtr device,
                       GLibInterface *glib)
    : IPConfig(device),
      provider_(provider),
      pid_(0),
      glib_(glib) {
  VLOG(2) << __func__ << ": " << GetDeviceName();
}

DHCPConfig::~DHCPConfig() {
  VLOG(2) << __func__ << ": " << GetDeviceName();
}

bool DHCPConfig::Request() {
  VLOG(2) << __func__ << ": " << GetDeviceName();
  if (!pid_) {
    return Start();
  }
  if (!proxy_.get()) {
    LOG(ERROR)
        << "Unable to acquire destination address before receiving request.";
    return false;
  }
  return Renew();
}

bool DHCPConfig::Renew() {
  VLOG(2) << __func__ << ": " << GetDeviceName();
  if (!pid_ || !proxy_.get()) {
    return false;
  }
  proxy_->DoRebind(GetDeviceName());
  return true;
}

void DHCPConfig::InitProxy(DBus::Connection *connection, const char *service) {
  if (!proxy_.get()) {
    proxy_.reset(new DHCPCDProxy(connection, service));
  }
}

void DHCPConfig::ProcessEventSignal(const std::string &reason,
                                    const Configuration &configuration) {
  LOG(INFO) << "Event reason: " << reason;

  IPConfig::Properties properties;
  if (!ParseConfiguration(configuration, &properties)) {
    LOG(ERROR) << "Unable to parse the new DHCP configuration -- ignored.";
    return;
  }
  UpdateProperties(properties);
}

bool DHCPConfig::Start() {
  VLOG(2) << __func__ << ": " << GetDeviceName();

  char *argv[4], *envp[1];
  argv[0] = const_cast<char *>(kDHCPCDPath);
  argv[1] = const_cast<char *>("-B");  // foreground
  argv[2] = const_cast<char *>(GetDeviceName().c_str());
  argv[3] = NULL;

  envp[0] = NULL;

  GPid pid = 0;
  if (!glib_->SpawnAsync(NULL,
                         argv,
                         envp,
                         G_SPAWN_DO_NOT_REAP_CHILD,
                         NULL,
                         NULL,
                         &pid,
                         NULL)) {
    LOG(ERROR) << "Unable to spawn " << kDHCPCDPath;
    return false;
  }
  pid_ = pid;
  LOG(INFO) << "Spawned " << kDHCPCDPath << " with pid: " << pid_;
  provider_->BindPID(pid_, this);
  // TODO(petkov): Add an exit watch to cleanup w/ g_spawn_close_pid.
  return true;
}

string DHCPConfig::GetIPv4AddressString(unsigned int address) {
  char str[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &address, str, arraysize(str))) {
    return str;
  }
  LOG(ERROR) << "Unable to convert IPv4 address to string: " << address;
  return "";
}

bool DHCPConfig::ParseConfiguration(const Configuration& configuration,
                                    IPConfig::Properties *properties) {
  VLOG(2) << __func__;
  for (Configuration::const_iterator it = configuration.begin();
       it != configuration.end(); ++it) {
    const string &key = it->first;
    const DBus::Variant &value = it->second;
    VLOG(2) << "Processing key: " << key;
    if (key == kConfigurationKeyIPAddress) {
      properties->address = GetIPv4AddressString(value.reader().get_uint32());
      if (properties->address.empty()) {
        return false;
      }
    } else if (key == kConfigurationKeySubnetCIDR) {
      properties->subnet_cidr = value.reader().get_byte();
    } else if (key == kConfigurationKeyBroadcastAddress) {
      properties->broadcast_address =
          GetIPv4AddressString(value.reader().get_uint32());
      if (properties->broadcast_address.empty()) {
        return false;
      }
    } else if (key == kConfigurationKeyRouters) {
      vector<unsigned int> routers = value.operator vector<unsigned int>();
      if (routers.empty()) {
        LOG(ERROR) << "No routers provided.";
        return false;
      }
      properties->gateway = GetIPv4AddressString(routers[0]);
      if (properties->gateway.empty()) {
        return false;
      }
    } else if (key == kConfigurationKeyDNS) {
      vector<unsigned int> servers = value.operator vector<unsigned int>();
      for (vector<unsigned int>::const_iterator it = servers.begin();
           it != servers.end(); ++it) {
        string server = GetIPv4AddressString(*it);
        if (server.empty()) {
          return false;
        }
        properties->dns_servers.push_back(server);
      }
    } else if (key == kConfigurationKeyDomainName) {
      properties->domain_name = value.reader().get_string();
    } else if (key == kConfigurationKeyDomainSearch) {
      properties->domain_search = value.operator vector<string>();
    } else if (key == kConfigurationKeyMTU) {
      int mtu = value.reader().get_uint16();
      if (mtu >= 576) {
        properties->mtu = mtu;
      }
    } else {
      VLOG(2) << "Key ignored.";
    }
  }
  return true;
}

}  // namespace shill
