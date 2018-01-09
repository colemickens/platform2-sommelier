// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/launcher/vsock_cid.h"

#include <stdint.h>
#include <sys/types.h>

#include <cinttypes>
#include <memory>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/stringprintf.h>

namespace vm_tools {
namespace launcher {

VsockCid::VsockCid(const base::FilePath& instance_runtime_dir,
                   bool release_on_destruction)
    : PooledResource(instance_runtime_dir, release_on_destruction) {}

VsockCid::~VsockCid() {
  if (ShouldReleaseOnDestruction() && !Release())
    LOG(ERROR) << "Failed to Release() vsock cid";
}

std::unique_ptr<VsockCid> VsockCid::Create(
    const base::FilePath& instance_runtime_dir) {
  auto cid =
      std::unique_ptr<VsockCid>(new VsockCid(instance_runtime_dir, true));
  if (!cid->Allocate())
    return nullptr;

  return cid;
}

std::unique_ptr<VsockCid> VsockCid::Load(
    const base::FilePath& instance_runtime_dir) {
  auto vsock_cid =
      std::unique_ptr<VsockCid>(new VsockCid(instance_runtime_dir, false));
  if (!vsock_cid->LoadInstance())
    return nullptr;

  return vsock_cid;
}

unsigned int VsockCid::GetCid() const {
  return selected_cid_;
}

const char* VsockCid::GetName() const {
  return "cid";
}

const std::string VsockCid::GetResourceID() const {
  return base::StringPrintf("%" PRIu64, selected_cid_);
}

bool VsockCid::LoadGlobalResources(const std::string& resources) {
  std::vector<std::string> lines = base::SplitString(
      resources, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  used_cids_.reset();

  for (const auto& line : lines) {
    uint64_t id;

    if (!base::StringToUint64(line, &id)) {
      LOG(ERROR) << "Failed to read cid";
      used_cids_.reset();
      return false;
    }

    if (id >= used_cids_.size()) {
      LOG(ERROR) << "VsockCid id " << id
                 << " is greater than the available number of cids";
      used_cids_.reset();
      return false;
    }

    if (used_cids_.test(id))
      LOG(WARNING) << "VsockCid " << id << " was used twice";

    used_cids_.set(id);
  }

  // Cids 0 and 1 are reserved. 2 belongs to the host.
  used_cids_.set(0);
  used_cids_.set(1);
  used_cids_.set(2);

  return true;
}

std::string VsockCid::PersistGlobalResources() {
  std::string resources;

  for (size_t i = 0; i < used_cids_.size(); i++) {
    if (used_cids_.test(i))
      base::StringAppendF(&resources, "%zu\n", i);
  }

  return resources;
}

bool VsockCid::LoadInstanceResource(const std::string& resource) {
  uint64_t cid;
  if (!base::StringToUint64(resource, &cid))
    return false;

  if (!IsCidAllocated(cid))
    return false;

  selected_cid_ = cid;
  return true;
}

bool VsockCid::AllocateResource() {
  uint64_t i;
  for (i = 0; i < used_cids_.size(); i++) {
    if (!used_cids_.test(i))
      break;
  }

  if (i == used_cids_.size()) {
    LOG(ERROR) << "No free cids to use";
    return false;
  }

  selected_cid_ = i;
  used_cids_.set(selected_cid_);

  return true;
}

bool VsockCid::ReleaseResource() {
  used_cids_.reset(selected_cid_);

  return true;
}

bool VsockCid::IsCidAllocated(const unsigned int cid) const {
  return used_cids_.test(cid);
}

}  // namespace launcher
}  // namespace vm_tools
