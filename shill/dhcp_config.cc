// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcp_config.h"

#include <arpa/inet.h>
#include <sys/wait.h>

#include <base/bind.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/dhcpcd_proxy.h"
#include "shill/dhcp_provider.h"
#include "shill/event_dispatcher.h"
#include "shill/glib.h"
#include "shill/ip_address.h"
#include "shill/proxy_factory.h"

using base::Bind;
using std::string;
using std::vector;

namespace shill {

// static
const char DHCPConfig::kConfigurationKeyBroadcastAddress[] = "BroadcastAddress";
const char DHCPConfig::kConfigurationKeyDNS[] = "DomainNameServers";
const char DHCPConfig::kConfigurationKeyDomainName[] = "DomainName";
const char DHCPConfig::kConfigurationKeyDomainSearch[] = "DomainSearch";
const char DHCPConfig::kConfigurationKeyIPAddress[] = "IPAddress";
const char DHCPConfig::kConfigurationKeyMTU[] = "InterfaceMTU";
const char DHCPConfig::kConfigurationKeyRouters[] = "Routers";
const char DHCPConfig::kConfigurationKeySubnetCIDR[] = "SubnetCIDR";
const int DHCPConfig::kDHCPCDExitPollMilliseconds = 50;
const int DHCPConfig::kDHCPCDExitWaitMilliseconds = 3000;
const char DHCPConfig::kDHCPCDPath[] = "/sbin/dhcpcd";
const char DHCPConfig::kDHCPCDPathFormatLease[] = "var/run/dhcpcd-%s.lease";
const char DHCPConfig::kDHCPCDPathFormatPID[] = "var/run/dhcpcd-%s.pid";
const int DHCPConfig::kMinMTU = 576;
const char DHCPConfig::kReasonBound[] = "BOUND";
const char DHCPConfig::kReasonFail[] = "FAIL";
const char DHCPConfig::kReasonRebind[] = "REBIND";
const char DHCPConfig::kReasonReboot[] = "REBOOT";
const char DHCPConfig::kReasonRenew[] = "RENEW";
// static
const char DHCPConfig::kType[] = "dhcp";


DHCPConfig::DHCPConfig(ControlInterface *control_interface,
                       EventDispatcher *dispatcher,
                       DHCPProvider *provider,
                       const string &device_name,
                       const string &request_hostname,
                       GLib *glib)
    : IPConfig(control_interface, device_name, kType),
      proxy_factory_(ProxyFactory::GetInstance()),
      provider_(provider),
      request_hostname_(request_hostname),
      pid_(0),
      child_watch_tag_(0),
      root_("/"),
      dispatcher_(dispatcher),
      glib_(glib) {
  mutable_store()->RegisterConstString(flimflam::kAddressProperty,
                                       &(properties().address));
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
  proxy_->Rebind(device_name());
  return true;
}

bool DHCPConfig::ReleaseIP() {
  VLOG(2) << __func__ << ": " << device_name();
  if (!pid_) {
    return true;
  }
  if (proxy_.get()) {
    proxy_->Release(device_name());
  }
  Stop();
  return true;
}

void DHCPConfig::InitProxy(const string &service) {
  // Defer proxy creation because dbus-c++ doesn't allow registration of new
  // D-Bus objects in the context of a D-Bus signal handler.
  if (!proxy_.get()) {
    dispatcher_->PostTask(
        Bind(&DHCPConfig::InitProxyTask, AsWeakPtr(), service));
  }
}

void DHCPConfig::InitProxyTask(const string &service) {
  if (!proxy_.get()) {
    VLOG(2) << "Init DHCP Proxy: " << device_name() << " at " << service;
    proxy_.reset(proxy_factory_->CreateDHCPProxy(service));
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

  vector<char *> args;
  args.push_back(const_cast<char *>(kDHCPCDPath));
  args.push_back(const_cast<char *>("-B"));  // foreground
  args.push_back(const_cast<char *>(device_name().c_str()));
  if (!request_hostname_.empty()) {
    args.push_back(const_cast<char *>("-h"));  // request hostname
    args.push_back(const_cast<char *>(request_hostname_.c_str()));
  }
  args.push_back(NULL);
  char *envp[1] = { NULL };

  CHECK(!pid_);
  if (!glib_->SpawnAsync(NULL,
                         args.data(),
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
    if (kill(pid_, SIGTERM) < 0) {
      PLOG(ERROR);
      return;
    }
    pid_t ret;
    int num_iterations =
        kDHCPCDExitWaitMilliseconds / kDHCPCDExitPollMilliseconds;
    for (int count = 0; count < num_iterations; ++count) {
      ret = waitpid(pid_, NULL, WNOHANG);
      if (ret == pid_ || ret == -1)
        break;
      usleep(kDHCPCDExitPollMilliseconds * 1000);
      if (count == num_iterations / 2)  // Make one last attempt to kill dhcpcd.
        kill(pid_, SIGKILL);
    }
    if (ret != pid_)
      PLOG(ERROR);
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
  properties->method = flimflam::kTypeDHCP;
  properties->address_family = IPAddress::kFamilyIPv4;
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
      properties->subnet_prefix = value.reader().get_byte();
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
      if (mtu >= kMinMTU) {
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
