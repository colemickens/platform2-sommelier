// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcp_config.h"

#include <arpa/inet.h>

#include <base/file_util.h>
#include <base/logging.h>
#include <base/stringprintf.h>

#include "shill/dhcpcd_proxy.h"
#include "shill/dhcp_provider.h"
#include "shill/glib.h"

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
const char DHCPConfig::kDHCPCDPathFormatLease[] = "var/run/dhcpcd-%s.lease";
const char DHCPConfig::kDHCPCDPathFormatPID[] = "var/run/dhcpcd-%s.pid";
const char DHCPConfig::kReasonBound[] = "BOUND";
const char DHCPConfig::kReasonFail[] = "FAIL";
const char DHCPConfig::kReasonRebind[] = "REBIND";
const char DHCPConfig::kReasonReboot[] = "REBOOT";
const char DHCPConfig::kReasonRenew[] = "RENEW";


DHCPConfig::DHCPConfig(DHCPProvider *provider,
                       const string &device_name,
                       GLib *glib)
    : IPConfig(device_name),
      provider_(provider),
      pid_(0),
      child_watch_tag_(0),
      root_("/"),
      glib_(glib) {
  VLOG(2) << __func__ << ": " << device_name;
}

DHCPConfig::~DHCPConfig() {
  VLOG(2) << __func__ << ": " << device_name();

  // Don't leave behind dhcpcd running.
  Stop();

  // Make sure we don't get any callbacks to the destroyed instance.
  CleanupClientState();
}

bool DHCPConfig::RequestIP() {
  VLOG(2) << __func__ << ": " << device_name();
  if (!pid_) {
    return Start();
  }
  if (!proxy_.get()) {
    LOG(ERROR) << "Unable to request IP before acquiring destination.";
    return Restart();
  }
  return RenewIP();
}

bool DHCPConfig::RenewIP() {
  VLOG(2) << __func__ << ": " << device_name();
  if (!pid_) {
    return false;
  }
  if (!proxy_.get()) {
    LOG(ERROR) << "Unable to renew IP before acquiring destination.";
    return false;
  }
  proxy_->DoRebind(device_name());
  return true;
}

bool DHCPConfig::ReleaseIP() {
  VLOG(2) << __func__ << ": " << device_name();
  if (!pid_) {
    return true;
  }
  if (!proxy_.get()) {
    LOG(ERROR) << "Unable to release IP before acquiring destination.";
    return false;
  }
  proxy_->DoRelease(device_name());
  Stop();
  return true;
}

void DHCPConfig::InitProxy(DBus::Connection *connection, const char *service) {
  if (!proxy_.get()) {
    proxy_.reset(new DHCPCDProxy(connection, service));
  }
}

void DHCPConfig::ProcessEventSignal(const string &reason,
                                    const Configuration &configuration) {
  LOG(INFO) << "Event reason: " << reason;
  if (reason == kReasonFail) {
    LOG(ERROR) << "Received failure event from DHCP client.";
    UpdateProperties(IPConfig::Properties(), false);
    return;
  }
  if (reason != kReasonBound &&
      reason != kReasonRebind &&
      reason != kReasonReboot &&
      reason != kReasonRenew) {
    LOG(WARNING) << "Event ignored.";
    return;
  }
  IPConfig::Properties properties;
  CHECK(ParseConfiguration(configuration, &properties));
  UpdateProperties(properties, true);
}

bool DHCPConfig::Start() {
  VLOG(2) << __func__ << ": " << device_name();

  char *argv[4] = {
    const_cast<char *>(kDHCPCDPath),
    const_cast<char *>("-B"),  // foreground
    const_cast<char *>(device_name().c_str()),
    NULL
  };
  char *envp[1] = { NULL };

  CHECK(!pid_);
  if (!glib_->SpawnAsync(NULL,
                         argv,
                         envp,
                         G_SPAWN_DO_NOT_REAP_CHILD,
                         NULL,
                         NULL,
                         &pid_,
                         NULL)) {
    LOG(ERROR) << "Unable to spawn " << kDHCPCDPath;
    return false;
  }
  LOG(INFO) << "Spawned " << kDHCPCDPath << " with pid: " << pid_;
  provider_->BindPID(pid_, this);
  CHECK(!child_watch_tag_);
  child_watch_tag_ = glib_->ChildWatchAdd(pid_, ChildWatchCallback, this);
  return true;
}

void DHCPConfig::Stop() {
  if (pid_) {
    VLOG(2) << "Terminating " << pid_;
    PLOG_IF(ERROR, kill(pid_, SIGTERM) < 0);
  }
}

bool DHCPConfig::Restart() {
  // Check to ensure that this instance doesn't get destroyed in the middle of
  // this call. If stopping a running client while there's only one reference to
  // this instance, we will end up destroying it when the PID is unbound from
  // the Provider. Since the Provider doesn't invoke Restart, this would mean
  // that Restart was erroneously executed through a bare reference.
  CHECK(!pid_ || !HasOneRef());
  Stop();
  if (pid_) {
    provider_->UnbindPID(pid_);
  }
  CleanupClientState();
  return Start();
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
  properties->address_family = IPConfig::kAddressFamilyIPv4;
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

void DHCPConfig::ChildWatchCallback(GPid pid, gint status, gpointer data) {
  VLOG(2) << "pid " << pid << " exit status " << status;
  DHCPConfig *config = reinterpret_cast<DHCPConfig *>(data);
  config->child_watch_tag_ = 0;
  CHECK_EQ(pid, config->pid_);
  config->CleanupClientState();

  // |config| instance may be destroyed after this call.
  config->provider_->UnbindPID(pid);
}

void DHCPConfig::CleanupClientState() {
  if (child_watch_tag_) {
    glib_->SourceRemove(child_watch_tag_);
    child_watch_tag_ = 0;
  }
  if (pid_) {
    glib_->SpawnClosePID(pid_);
    pid_ = 0;
  }
  proxy_.reset();
  file_util::Delete(root_.Append(base::StringPrintf(kDHCPCDPathFormatLease,
                                                    device_name().c_str())),
                    false);
  file_util::Delete(root_.Append(base::StringPrintf(kDHCPCDPathFormatPID,
                                                    device_name().c_str())),
                    false);
}

}  // namespace shill
