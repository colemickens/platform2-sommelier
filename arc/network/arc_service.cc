// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/arc_service.h"

#include <linux/rtnetlink.h>
#include <net/if.h>

#include <memory>
#include <string>
#include <utility>

#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <shill/net/rtnl_message.h>

#include "arc/network/datapath.h"
#include "arc/network/ipc.pb.h"
#include "arc/network/mac_address_generator.h"
#include "arc/network/net_util.h"
#include "arc/network/scoped_ns.h"

namespace arc_networkd {
namespace {
constexpr pid_t kInvalidPID = -1;
constexpr pid_t kTestPID = -2;

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

// TODO(garrick): Remove this workaround ASAP.
int GetContainerPID() {
  const base::FilePath path("/run/containers/android-run_oci/container.pid");
  std::string pid_str;
  if (!base::ReadFileToStringWithMaxSize(path, &pid_str, 16 /* max size */)) {
    LOG(ERROR) << "Failed to read pid file";
    return kInvalidPID;
  }
  int pid;
  if (!base::StringToInt(base::TrimWhitespaceASCII(pid_str, base::TRIM_ALL),
                         &pid)) {
    LOG(ERROR) << "Failed to convert container pid string";
    return kInvalidPID;
  }
  LOG(INFO) << "Read container pid as " << pid;
  return pid;
}

}  // namespace

ArcService::ArcService(DeviceManagerBase* dev_mgr,
                       bool is_legacy,
                       std::unique_ptr<Datapath> datapath)
    : GuestService(is_legacy ? GuestMessage::ARC_LEGACY : GuestMessage::ARC,
                   dev_mgr),
      pid_(kInvalidPID) {
  if (!datapath)
    datapath = std::make_unique<Datapath>(&runner_);

  datapath_ = std::move(datapath);
}

void ArcService::OnStart() {
  LOG(INFO) << "ARC++ network service starting";
  pid_ = GetContainerPID();
  if (pid_ == kInvalidPID) {
    LOG(ERROR) << "Cannot start service - invalid container PID";
    return;
  }

  // Start listening for RTNetlink messages in the container's net namespace
  // to be notified whenever it brings up an interface.
  {
    ScopedNS ns(pid_);
    if (ns.IsValid()) {
      rtnl_handler_ = std::make_unique<RTNetlinkHandler>();
      rtnl_handler_->Start(RTMGRP_LINK);
      link_listener_ = std::make_unique<shill::RTNLListener>(
          shill::RTNLHandler::kRequestLink,
          Bind(&ArcService::LinkMsgHandler, weak_factory_.GetWeakPtr()),
          rtnl_handler_.get());
    } else {
      // This is bad - it means we won't ever be able to tell when the container
      // brings up an interface.
      LOG(ERROR)
          << "Cannot start netlink listener - invalid container namespace?";
    }
  }

  // Add all known host devices.
  if (guest_ != GuestMessage::ARC_LEGACY) {
    dev_mgr_->ProcessDevices(
        base::Bind(&ArcService::OnDeviceAdded, weak_factory_.GetWeakPtr()));
  }

  // This will invoke the similar call on DeviceManager that will add the
  // Android device, which, in turn, calls back into OnDeviceAdded here.
  GuestService::OnStart();

  GuestMessage msg;
  msg.set_event(GuestMessage::START);
  msg.set_arc_pid(pid_);
  msg.set_type(guest_);
  DispatchMessage(msg);
}

void ArcService::OnStop() {
  LOG(INFO) << "ARC++ network service stopping";
  // This will invoke the similar call on DeviceManager that will remove the
  // Android device, which, in turn, calls back into OnDeviceRemoved here.
  GuestService::OnStop();

  // Remove any other devices.
  if (guest_ != GuestMessage::ARC_LEGACY) {
    dev_mgr_->ProcessDevices(
        base::Bind(&ArcService::OnDeviceRemoved, weak_factory_.GetWeakPtr()));
  }

  link_listener_.reset();
  rtnl_handler_.reset();

  GuestMessage msg;
  msg.set_event(GuestMessage::STOP);
  msg.set_type(guest_);
  DispatchMessage(msg);

  pid_ = kInvalidPID;
}

void ArcService::OnDeviceAdded(Device* device) {
  // ARC N uses legacy single networking and only requires the arcbr0/arc0
  // configuration. Any other device can be safely ignored.
  if (guest_ == GuestMessage::ARC_LEGACY && !device->IsLegacyAndroid())
    return;

  // If ARC isn't running, there is nothing to do. This call must have been
  // triggered by a device hot-plug event or something similar in DeviceManager.
  // It's OK to ignore because we'll still get the new device when the container
  // starts up.
  if (pid_ == kInvalidPID)
    return;

  const auto& config = device->config();

  LOG(INFO) << "Setting up " << device->ifname()
            << " bridge: " << config.host_ifname()
            << " guest_iface: " << config.guest_ifname()
            << " for container pid " << pid_;

  // Create the bridge.
  if (!datapath_->AddBridge(config.host_ifname(),
                            IPv4AddressToString(config.host_ipv4_addr()))) {
    LOG(ERROR) << "Failed to setup arc bridge: " << config.host_ifname();
    return;
  }

  // Setup the iptables.
  if (device->IsLegacyAndroid()) {
    if (!datapath_->AddLegacyIPv4DNAT(
            IPv4AddressToString(config.guest_ipv4_addr())))
      LOG(ERROR) << "Failed to configure ARC traffic rules";

    if (!datapath_->AddOutboundIPv4(config.host_ifname()))
      LOG(ERROR) << "Failed to configure egress traffic rules";
  } else if (!device->IsAndroid()) {
    if (!datapath_->AddInboundIPv4DNAT(
            device->ifname(), IPv4AddressToString(config.guest_ipv4_addr())))
      LOG(ERROR) << "Failed to configure ingress traffic rules for "
                 << device->ifname();

    if (!datapath_->AddOutboundIPv4(config.host_ifname()))
      LOG(ERROR) << "Failed to configure egress traffic rules";
  }

  // Finish the data path by wiring up the veth pair.
  std::string veth_ifname = datapath_->AddVirtualBridgedInterface(
      device->ifname(), MacAddressToString(config.guest_mac_addr()),
      config.host_ifname());
  if (veth_ifname.empty()) {
    LOG(ERROR) << "Failed to create virtual interface for container";
    return;
  }

  if (!datapath_->AddInterfaceToContainer(
          pid_, veth_ifname, config.guest_ifname(),
          IPv4AddressToString(config.guest_ipv4_addr()),
          device->options().fwd_multicast)) {
    LOG(ERROR) << "Failed to create container interface.";
    datapath_->RemoveInterface(veth_ifname);
    datapath_->RemoveBridge(config.host_ifname());
    return;
  }

  // Signal the container that the network device is ready.
  // This is only applicable for arc0.
  if (device->IsAndroid() || device->IsLegacyAndroid()) {
    MinijailedProcessRunner runner;
    runner.WriteSentinelToContainer(base::IntToString(pid_));
  }
}

void ArcService::OnDeviceRemoved(Device* device) {
  // ARC N uses legacy single networking and only requires the arcbr0/arc0
  // configuration. Any other device can be safely ignored.
  if (guest_ == GuestMessage::ARC_LEGACY && !device->IsLegacyAndroid())
    return;

  // If ARC isn't running, there is nothing to do. This call must have been
  // triggered by a device hot-plug event or something similar in DeviceManager.
  // It's OK to ignore because if the container is gone there is nothing to do.
  if (pid_ == kInvalidPID)
    return;

  const auto& config = device->config();

  LOG(INFO) << "Tearing down " << device->ifname()
            << " bridge: " << config.host_ifname()
            << " guest_iface: " << config.guest_ifname();

  if (device->IsLegacyAndroid()) {
    datapath_->RemoveOutboundIPv4(config.host_ifname());
    datapath_->RemoveLegacyIPv4DNAT();
  } else if (!device->IsAndroid()) {
    datapath_->RemoveOutboundIPv4(config.host_ifname());
    datapath_->RemoveInboundIPv4DNAT(
        device->ifname(), IPv4AddressToString(config.guest_ipv4_addr()));

    datapath_->RemoveInterface(ArcVethHostName(device->ifname()));
  }

  datapath_->RemoveBridge(config.host_ifname());
}

void ArcService::OnDefaultInterfaceChanged(const std::string& ifname) {
  if (pid_ == kInvalidPID || guest_ != GuestMessage::ARC_LEGACY)
    return;

  datapath_->RemoveLegacyIPv4InboundDNAT();

  auto* device = dev_mgr_->FindByGuestInterface("arc0");
  if (!device) {
    LOG(DFATAL) << "Expected legacy Android device missing";
    return;
  }

  device->Disable();
  if (!ifname.empty()) {
    datapath_->AddLegacyIPv4InboundDNAT(ifname);
    device->Enable(ifname);
  }
}

void ArcService::LinkMsgHandler(const shill::RTNLMessage& msg) {
  if (!msg.HasAttribute(IFLA_IFNAME)) {
    LOG(ERROR) << "Link event message does not have IFLA_IFNAME";
    return;
  }
  bool link_up = msg.link_status().flags & IFF_UP;
  shill::ByteString b(msg.GetAttribute(IFLA_IFNAME));
  std::string ifname(reinterpret_cast<const char*>(
      b.GetSubstring(0, IFNAMSIZ).GetConstData()));

  auto* device = dev_mgr_->FindByGuestInterface(ifname);
  if (!device || !device->LinkUp(ifname, link_up))
    return;

  if (!link_up) {
    LOG(INFO) << ifname << " is now down";
    return;
  }
  if (device->IsAndroid())
    return;

  LOG(INFO) << ifname << " is now up";
  const auto& default_ifname = dev_mgr_->DefaultInterface();
  device->Enable(device->IsLegacyAndroid() && !default_ifname.empty()
                     ? default_ifname
                     : ifname);
}

void ArcService::SetPIDForTestingOnly() {
  pid_ = kTestPID;
}

}  // namespace arc_networkd
