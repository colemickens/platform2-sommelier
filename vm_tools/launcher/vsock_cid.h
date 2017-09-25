// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_LAUNCHER_VSOCK_CID_H_
#define VM_TOOLS_LAUNCHER_VSOCK_CID_H_

#include <bitset>
#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <base/macros.h>

#include "vm_tools/launcher/pooled_resource.h"

namespace vm_tools {
namespace launcher {

// Manages available vsock CIDs.
class VsockCid : public PooledResource {
 public:
  VsockCid(const base::FilePath& instance_runtime_dir,
           bool release_on_destruction);
  ~VsockCid() override;

  unsigned int GetCid() const;

  static std::unique_ptr<VsockCid> Create(
      const base::FilePath& instance_runtime_dir);
  static std::unique_ptr<VsockCid> Load(
      const base::FilePath& instance_runtime_dir);

 protected:
  // PooledResource overrides
  const char* GetName() const override;
  const std::string GetResourceID() const override;
  bool LoadGlobalResources(const std::string& resources) override;
  std::string PersistGlobalResources() override;
  bool LoadInstanceResource(const std::string& resource) override;
  bool AllocateResource() override;
  bool ReleaseResource() override;

 private:
  bool IsCidAllocated(const unsigned int cid) const;

  // Cids 0 and 1 are reserved. Cid 2 belongs to the host.
  std::bitset<256> used_cids_;
  uint64_t selected_cid_;

  DISALLOW_COPY_AND_ASSIGN(VsockCid);
};
}  // namespace launcher
}  // namespace vm_tools

#endif  // VM_TOOLS_LAUNCHER_VSOCK_CID_H_
