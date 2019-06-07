// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/arc_helper.h"

#include <ctype.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <unistd.h>

#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <shill/net/rtnl_handler.h>
#include <shill/net/rtnl_message.h>

#include "arc/network/minijailed_process_runner.h"
#include "arc/network/scoped_ns.h"

namespace {

const char kContainerPIDPath[] =
    "/run/containers/android-run_oci/container.pid";

int GetContainerPID() {
  const base::FilePath path(kContainerPIDPath);
  std::string pid_str;
  if (!base::ReadFileToStringWithMaxSize(path, &pid_str, 16 /* max size */)) {
    LOG(ERROR) << "Failed to read pid file";
    return -1;
  }
  int pid;
  if (!base::StringToInt(base::TrimWhitespaceASCII(pid_str, base::TRIM_ALL),
                         &pid)) {
    LOG(ERROR) << "Failed to convert container pid string";
    return -1;
  }
  LOG(INFO) << "Read container pid as " << pid;
  return pid;
}

// This wrapper is required since the base class is a singleton that hides its
// constructor. It is necessary here because the message loop thread has to be
// reassociated to the container's network namespace; and since the container
// can be repeatedly created and destroyed, the handler must be as well.
class RTNetlinkHandler : public shill::RTNLHandler {
 public:
  RTNetlinkHandler() = default;
  ~RTNetlinkHandler() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(RTNetlinkHandler);
};

}  // namespace

namespace arc_networkd {

// static
std::unique_ptr<ArcHelper> ArcHelper::New() {
  // Load networking modules needed by Android that are not compiled in the
  // kernel. Android does not allow auto-loading of kernel modules.
  auto process_runner = std::make_unique<MinijailedProcessRunner>();

  // These must succeed.
  if (process_runner->ModprobeAll({
          // The netfilter modules needed by netd for iptables commands.
          "ip6table_filter",
          "ip6t_ipv6header",
          "ip6t_REJECT",
          // The xfrm modules needed for Android's ipsec APIs.
          "xfrm4_mode_transport",
          "xfrm4_mode_tunnel",
          "xfrm6_mode_transport",
          "xfrm6_mode_tunnel",
          // The ipsec modules for AH and ESP encryption for ipv6.
          "ah6",
          "esp6",
      }) != 0) {
    LOG(ERROR) << "One or more required kernel modules failed to load.";
    return nullptr;
  }

  // Optional modules.
  if (process_runner->ModprobeAll({
          // This module is not available in kernels < 3.18
          "nf_reject_ipv6",
          // These modules are needed for supporting Chrome traffic on Android
          // VPN which uses Android's NAT feature. Android NAT sets up iptables
          // rules that use these conntrack modules for FTP/TFTP.
          "nf_nat_ftp",
          "nf_nat_tftp",
      }) != 0) {
    LOG(INFO) << "One or more optional kernel modules failed to load.";
  }

  return base::WrapUnique(new ArcHelper);
}

void ArcHelper::Start() {
  LOG(INFO) << "Container starting";
  pid_ = GetContainerPID();
  CHECK_NE(pid_, 0);

  // Start listening for RTNetlink messages in the container's net namespace
  // to be notified whenever it brings up an interface.
  {
    ScopedNS ns(pid_);
    if (!ns.IsValid()) {
      // This is kind of bad - it means we won't ever be able to tell when
      // the container brings up an interface.
      LOG(ERROR) << "Cannot start netlink listener";
      return;
    }

    rtnl_handler_ = std::make_unique<RTNetlinkHandler>();
    rtnl_handler_->Start(RTMGRP_LINK);
    link_listener_ = std::make_unique<shill::RTNLListener>(
        shill::RTNLHandler::kRequestLink,
        Bind(&ArcHelper::LinkMsgHandler, weak_factory_.GetWeakPtr()),
        rtnl_handler_.get());
  }

  // Initialize the container interfaces.
  for (auto& config : arc_ip_configs_) {
    config.second->Init(pid_);
  }
}

void ArcHelper::Stop() {
  LOG(INFO) << "Container stopping";

  link_listener_.reset();
  rtnl_handler_.reset();

  // Reset the container interfaces.
  for (auto& config : arc_ip_configs_) {
    config.second->Init(0);
  }

  pid_ = 0;
}

void ArcHelper::AddDevice(const std::string& ifname,
                          const DeviceConfig& config) {
  LOG(INFO) << "Adding device " << ifname;
  auto device = std::make_unique<ArcIpConfig>(ifname, config);
  // If the container is already up, then this device needs to be initialized.
  if (pid_ != 0)
    device->Init(pid_);

  configs_by_arc_ifname_.emplace(config.arc_ifname(), device.get());
  arc_ip_configs_.emplace(ifname, std::move(device));
}

void ArcHelper::RemoveDevice(const std::string& ifname) {
  LOG(INFO) << "Removing device " << ifname;
  configs_by_arc_ifname_.erase(ifname);
  arc_ip_configs_.erase(ifname);
}

void ArcHelper::HandleCommand(const IpHelperMessage& cmd) {
  const std::string& dev_ifname = cmd.dev_ifname();
  const auto it = arc_ip_configs_.find(dev_ifname);
  if (it == arc_ip_configs_.end()) {
    if (cmd.has_dev_config()) {
      AddDevice(dev_ifname, cmd.dev_config());
    } else {
      LOG(ERROR) << "Unexpected device " << dev_ifname;
    }
    return;
  }

  auto* config = it->second.get();
  if (cmd.has_clear_arc_ip()) {
    config->Clear();
  } else if (cmd.has_set_arc_ip()) {
    config->Set(cmd.set_arc_ip());
  } else if (cmd.has_enable_inbound_ifname()) {
    config->EnableInbound(cmd.enable_inbound_ifname());
  } else if (cmd.has_disable_inbound()) {
    config->DisableInbound();
  } else if (cmd.has_teardown()) {
    RemoveDevice(dev_ifname);
  }
}

void ArcHelper::LinkMsgHandler(const shill::RTNLMessage& msg) {
  if (!msg.HasAttribute(IFLA_IFNAME)) {
    LOG(ERROR) << "Link event message does not have IFLA_IFNAME";
    return;
  }
  bool link_up = msg.link_status().flags & IFF_UP;
  shill::ByteString b(msg.GetAttribute(IFLA_IFNAME));
  std::string ifname(reinterpret_cast<const char*>(
      b.GetSubstring(0, IFNAMSIZ).GetConstData()));
  auto it = configs_by_arc_ifname_.find(ifname);
  if (it != configs_by_arc_ifname_.end()) {
    it->second->ContainerReady(link_up);
  }
}

}  // namespace arc_networkd
