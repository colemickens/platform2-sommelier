// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/launcher/mac_address.h"

#include <sys/types.h>

#include <algorithm>
#include <memory>
#include <sstream>

#include <base/logging.h>
#include <base/rand_util.h>
#include <base/strings/string_split.h>
#include <base/strings/stringprintf.h>
#include <base/values.h>

namespace vm_tools {
namespace launcher {

namespace {

const char kMacAddressFormat[] = "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx";

MacAddress::Octets GenerateRandomMac() {
  MacAddress::Octets mac_addr;

  for (auto& it : mac_addr)
    it = base::RandGenerator(255);

  // Set the locally administered flag.
  mac_addr[0] |= 0x02u;

  // Unset the multicast flag.
  mac_addr[0] &= ~0x01u;

  return mac_addr;
}

std::string MacToString(const MacAddress::Octets& addr) {
  return base::StringPrintf(kMacAddressFormat, addr[0], addr[1], addr[2],
                            addr[3], addr[4], addr[5]);
}

bool StringToMac(MacAddress::Octets* result, const std::string& addr) {
  DCHECK(result);

  int rc = sscanf(addr.c_str(), kMacAddressFormat, &(*result)[0], &(*result)[1],
                  &(*result)[2], &(*result)[3], &(*result)[4], &(*result)[5]);

  if (rc != result->size()) {
    LOG(ERROR) << "Unable to parse MAC address";
    return false;
  }

  return true;
}

}  // namespace

MacAddress::MacAddress(const base::FilePath& instance_runtime_dir,
                       bool release_on_destruction)
    : PooledResource(instance_runtime_dir, release_on_destruction) {}

MacAddress::~MacAddress() {
  if (ShouldReleaseOnDestruction() && !Release())
    LOG(ERROR) << "Failed to Release() MAC address";
}

std::unique_ptr<MacAddress> MacAddress::Create(
    const base::FilePath& instance_runtime_dir) {
  auto addr =
      std::unique_ptr<MacAddress>(new MacAddress(instance_runtime_dir, true));
  if (!addr->Allocate())
    return nullptr;

  return addr;
}

std::unique_ptr<MacAddress> MacAddress::Load(
    const base::FilePath& instance_runtime_dir) {
  auto addr =
      std::unique_ptr<MacAddress>(new MacAddress(instance_runtime_dir, false));
  if (!addr->LoadInstance())
    return nullptr;

  return addr;
}

std::string MacAddress::ToString() const {
  return MacToString(selected_mac_);
}

const char* MacAddress::GetName() const {
  return "mac";
}

const std::string MacAddress::GetResourceID() const {
  return MacToString(selected_mac_);
}

bool MacAddress::LoadGlobalResources(const std::string& resources) {
  std::vector<std::string> lines = base::SplitString(
      resources, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  allocated_macs_.clear();

  for (const auto& line : lines) {
    Octets addr;

    if (!StringToMac(&addr, line)) {
      allocated_macs_.clear();
      return false;
    }

    allocated_macs_.push_back(addr);
  }

  return true;
}

std::string MacAddress::PersistGlobalResources() {
  std::string resources;

  for (const auto& mac : allocated_macs_)
    resources += MacToString(mac) + '\n';

  return resources;
}

bool MacAddress::LoadInstanceResource(const std::string& resource) {
  MacAddress::Octets octets;
  if (!StringToMac(&octets, resource))
    return false;

  if (!IsMacAllocated(octets))
    return false;

  selected_mac_ = octets;

  return true;
}

bool MacAddress::AllocateResource() {
  Octets candidate = GenerateRandomMac();

  while (!IsValidMac(candidate))
    candidate = GenerateRandomMac();

  selected_mac_ = candidate;

  allocated_macs_.push_back(selected_mac_);

  return true;
}

bool MacAddress::ReleaseResource() {
  auto it =
      std::find(allocated_macs_.begin(), allocated_macs_.end(), selected_mac_);
  if (it == allocated_macs_.end()) {
    LOG(ERROR) << "MAC address already removed from list of allocated MACs";
    return false;
  }

  allocated_macs_.erase(it);

  return true;
}

bool MacAddress::IsValidMac(const Octets& candidate) const {
  const std::vector<Octets> blacklisted_macs = {
      // Broadcast address
      {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}}};

  for (const auto& mac : blacklisted_macs) {
    if (mac == candidate)
      return false;
  }

  return !IsMacAllocated(candidate);
}

bool MacAddress::IsMacAllocated(const Octets& candidate) const {
  for (const auto& mac : allocated_macs_) {
    if (mac == candidate)
      return true;
  }
  return false;
}

}  // namespace launcher
}  // namespace vm_tools
