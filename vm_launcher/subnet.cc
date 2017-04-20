// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_launcher/subnet.h"

#include <stdint.h>
#include <sys/types.h>

#include <vector>

#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/stringprintf.h>

namespace vm_launcher {

namespace {
// The 100.115.92.0/24 subnet is reserved and cannot be publicly routed.
const char kIpFormat[] = "100.115.92.%hhu";
}  // namespace

Subnet::~Subnet() {
  if (!Release())
    LOG(ERROR) << "Unable to release subnet " << selected_subnet_;
}

std::unique_ptr<Subnet> Subnet::Create() {
  auto subnet = base::MakeUnique<Subnet>();
  if (!subnet->Allocate())
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
  return "subnets";
}

bool Subnet::LoadResources(const std::string& resources) {
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

std::string Subnet::PersistResources() {
  std::string resources;

  for (size_t i = 0; i < allocated_subnets_.size(); i++) {
    if (allocated_subnets_.test(i))
      resources += base::StringPrintf("%zu\n", i);
  }

  return resources;
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

}  // namespace vm_launcher
