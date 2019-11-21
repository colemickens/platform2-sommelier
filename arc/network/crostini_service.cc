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
    : GuestService(GuestMessage::TERMINA_VM, dev_mgr), datapath_(datapath) {}

bool CrostiniService::Start(int32_t cid) {
  if (cid <= kInvalidCID) {
    LOG(ERROR) << "Invalid VM cid " << cid;
    return false;
  }

  cids_.insert(cid);
  LOG(INFO) << "Crostini network service started for {cid: " << cid << "}";

  // Add a new Termina device.
  auto ctx = std::make_unique<Context>();
  ctx->SetCID(cid);
  dev_mgr_->AddWithContext(
      base::StringPrintf("%s%d", kTerminaVmDevicePrefix, cid), std::move(ctx));

  return true;
}

void CrostiniService::Stop(int32_t cid) {
  if (cids_.erase(cid) == 0) {
    LOG(WARNING) << "Unknown {cid: " << cid << "}";
    return;
  }

  // Remove the device.
  dev_mgr_->Remove(base::StringPrintf("%s%d", kTerminaVmDevicePrefix, cid));

  LOG(INFO) << "Crostini network service stopped for {cid: " << cid << "}";
}

void CrostiniService::OnDeviceAdded(Device* device) {
  if (!device->IsCrostini())
    return;

  auto ctx = dynamic_cast<Context*>(device->context());
  if (!ctx) {
    LOG(ERROR) << "Context missing for " << device->ifname();
    return;
  }

  LOG(INFO) << "Starting device " << device->ifname();

  const auto& config = device->config();
  const auto mac_addr = config.guest_mac_addr();
  std::string tap =
      datapath_->AddTAP("" /* auto-generate name */, &mac_addr,
                        config.host_ipv4_subnet_addr(), vm_tools::kCrosVmUser);
  if (tap.empty()) {
    LOG(ERROR) << "Failed to create TAP device for VM";
    return;
  }

  ctx->SetTAP(tap);
}

void CrostiniService::OnDeviceRemoved(Device* device) {
  if (!device->IsCrostini())
    return;

  auto ctx = dynamic_cast<Context*>(device->context());
  if (!ctx) {
    LOG(ERROR) << "Context missing for " << device->ifname();
    return;
  }

  datapath_->RemoveInterface(ctx->TAP());
}

// Context

bool CrostiniService::Context::IsLinkUp() const {
  // Unused.
  return true;
}

int32_t CrostiniService::Context::CID() const {
  return cid_;
}

void CrostiniService::Context::SetCID(int32_t cid) {
  cid_ = cid;
}

const std::string& CrostiniService::Context::TAP() const {
  return tap_;
}

void CrostiniService::Context::SetTAP(const std::string& tap) {
  tap_ = tap;
}

}  // namespace arc_networkd
