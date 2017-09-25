// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/launcher/subnet.h"

#include <stdint.h>
#include <sys/types.h>

#include <memory>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/stringprintf.h>

namespace vm_tools {
namespace launcher {

namespace {
// The 100.115.92.0/24 subnet is reserved and cannot be publicly routed.
const char kIpFormat[] = "100.115.92.%hhu";
}  // namespace

Subnet::Subnet(const base::FilePath& instance_runtime_dir,
               bool release_on_destruction)
    : PooledResource(instance_runtime_dir, release_on_destruction) {}

Subnet::~Subnet() {
  if (ShouldReleaseOnDestruction() && !Release())
    LOG(ERROR) << "Failed to Release() subnet";
}

std::unique_ptr<Subnet> Subnet::Create(
    const base::FilePath& instance_runtime_dir) {
  auto subnet = std::unique_ptr<Subnet>(new Subnet(instance_runtime_dir, true));
  if (!subnet->Allocate())
    return nullptr;

  return subnet;
}

std::unique_ptr<Subnet> Subnet::Load(
    const base::FilePath& instance_runtime_dir) {
  auto subnet =
      std::unique_ptr<Subnet>(new Subnet(instance_runtime_dir, false));
  if (!subnet->LoadInstance())
    return nullptr;

  return subnet;
}

std::string Subnet::GetGatewayAddress() const {
  uint8_t last_octet = selected_subnet_ * 4 + 1;

  return base::StringPrintf(kIpFormat, last_octet);
}

std::string Subnet::GetIpAddress() const {
  uint8_t last_octet = selected_subnet_ * 4 + 2;

  return base::StringPrintf(kIpFormat, last_octet);
}

// TODO(smbarber): Support variable size subnets.
std::string Subnet::GetNetmask() const {
  return "255.255.255.252";
}

const char* Subnet::GetName() const {
  return "subnet";
}

const std::string Subnet::GetResourceID() const {
  return base::StringPrintf("%zu", selected_subnet_);
}

bool Subnet::LoadGlobalResources(const std::string& resources) {
  std::vector<std::string> lines = base::SplitString(
      resources, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  allocated_subnets_.reset();

  for (const auto& line : lines) {
    size_t id;

    if (!base::StringToSizeT(line, &id)) {
      LOG(ERROR) << "Failed to read subnet ID";
      allocated_subnets_.reset();
      return false;
    }

    if (id >= allocated_subnets_.size()) {
      LOG(ERROR) << "Subnet id " << id
                 << " is greater than the available number of subnets";
      allocated_subnets_.reset();
      return false;
    }

    if (allocated_subnets_.test(id))
      LOG(WARNING) << "Subnet " << id << " was used twice";

    allocated_subnets_.set(id);
  }

  // First subnet is always reserved for ARC++.
  allocated_subnets_.set(0);

  return true;
}

std::string Subnet::PersistGlobalResources() {
  std::string resources;

  for (size_t i = 0; i < allocated_subnets_.size(); i++) {
    if (allocated_subnets_.test(i))
      resources += base::StringPrintf("%zu\n", i);
  }

  return resources;
}

bool Subnet::LoadInstanceResource(const std::string& resource) {
  size_t id;
  if (!base::StringToSizeT(resource, &id))
    return false;

  if (!IsSubnetAllocated(id))
    return false;

  selected_subnet_ = id;
  return true;
}

bool Subnet::AllocateResource() {
  size_t i;
  for (i = 0; i < allocated_subnets_.size(); i++) {
    if (!allocated_subnets_.test(i))
      break;
  }

  if (i == allocated_subnets_.size()) {
    LOG(ERROR) << "No free subnets to use";
    return false;
  }

  selected_subnet_ = i;
  allocated_subnets_.set(selected_subnet_);

  return true;
}

bool Subnet::ReleaseResource() {
  allocated_subnets_.reset(selected_subnet_);

  return true;
}

bool Subnet::IsSubnetAllocated(const size_t subnet_id) const {
  return allocated_subnets_.test(subnet_id);
}

}  // namespace launcher
}  // namespace vm_tools
