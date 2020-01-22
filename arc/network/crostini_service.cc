// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/crostini_service.h"

#include <memory>
#include <utility>

#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/constants/vm_tools.h>

#include "arc/network/device.h"

namespace arc_networkd {
namespace {
constexpr int32_t kInvalidCID = -1;
}  // namespace

CrostiniService::CrostiniService(DeviceManagerBase* dev_mgr, Datapath* datapath)
    : dev_mgr_(dev_mgr), datapath_(datapath) {
  DCHECK(dev_mgr_);
  DCHECK(datapath_);

  dev_mgr_->RegisterDefaultInterfaceChangedHandler(
      GuestMessage::TERMINA_VM,
      base::Bind(&CrostiniService::OnDefaultInterfaceChanged,
                 base::Unretained(this)));
}

bool CrostiniService::Start(int32_t cid) {
  if (cid <= kInvalidCID) {
    LOG(ERROR) << "Invalid VM cid " << cid;
    return false;
  }
  if (taps_.find(cid) != taps_.end()) {
    LOG(WARNING) << "Already started for {cid: " << cid << "}";
    return false;
  }

  if (!AddTAP(cid))
    return false;

  LOG(INFO) << "Crostini network service started for {cid: " << cid << "}";
  dev_mgr_->StartForwarding(*taps_[cid].get());
  return true;
}

void CrostiniService::Stop(int32_t cid) {
  const auto it = taps_.find(cid);
  if (it == taps_.end()) {
    LOG(WARNING) << "Unknown {cid: " << cid << "}";
    return;
  }

  const auto* dev = it->second.get();
  dev_mgr_->StopForwarding(*dev);
  datapath_->RemoveInterface(dev->config().host_ifname());
  taps_.erase(it);

  LOG(INFO) << "Crostini network service stopped for {cid: " << cid << "}";
}

const Device* const CrostiniService::TAP(int32_t cid) const {
  const auto it = taps_.find(cid);
  if (it == taps_.end()) {
    return nullptr;
  }
  return it->second.get();
}

bool CrostiniService::AddTAP(int32_t cid) {
  auto* const addr_mgr = dev_mgr_->addr_mgr();
  auto ipv4_subnet =
      addr_mgr->AllocateIPv4Subnet(AddressManager::Guest::VM_TERMINA);
  if (!ipv4_subnet) {
    LOG(ERROR)
        << "Subnet already in use or unavailable. Cannot make device for cid "
        << cid;
    return false;
  }
  auto host_ipv4_addr = ipv4_subnet->AllocateAtOffset(0);
  if (!host_ipv4_addr) {
    LOG(ERROR) << "Host address already in use or unavailable. Cannot make "
                  "device for cid "
               << cid;
    return false;
  }
  auto guest_ipv4_addr = ipv4_subnet->AllocateAtOffset(1);
  if (!guest_ipv4_addr) {
    LOG(ERROR) << "VM address already in use or unavailable. Cannot make "
                  "device for cid "
               << cid;
    return false;
  }
  auto lxd_subnet =
      addr_mgr->AllocateIPv4Subnet(AddressManager::Guest::CONTAINER);
  if (!lxd_subnet) {
    LOG(ERROR) << "lxd subnet already in use or unavailable."
               << " Cannot make device for cid " << cid;
    return false;
  }

  const auto mac_addr = addr_mgr->GenerateMacAddress();
  const std::string tap =
      datapath_->AddTAP("" /* auto-generate name */, &mac_addr,
                        host_ipv4_addr.get(), vm_tools::kCrosVmUser);
  if (tap.empty()) {
    LOG(ERROR) << "Failed to create TAP device for cid " << cid;
    return false;
  }

  auto config = std::make_unique<Device::Config>(
      tap, "", mac_addr, std::move(ipv4_subnet), std::move(host_ipv4_addr),
      std::move(guest_ipv4_addr), std::move(lxd_subnet));

  Device::Options opts{
      .fwd_multicast = true,
      .ipv6_enabled = true,
      .find_ipv6_routes_legacy = false,
      .use_default_interface = true,
      .is_android = false,
      .is_sticky = true,
  };

  taps_.emplace(cid, std::make_unique<Device>(tap, std::move(config), opts,
                                              GuestMessage::TERMINA_VM));
  return true;
}

void CrostiniService::OnDefaultInterfaceChanged(const std::string& ifname) {
  for (const auto& t : taps_)
    dev_mgr_->StopForwarding(*t.second.get());

  if (ifname.empty())
    return;

  for (const auto& t : taps_)
    dev_mgr_->StartForwarding(*t.second.get());
}

}  // namespace arc_networkd
