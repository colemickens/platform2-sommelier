// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_LAUNCHER_NFS_EXPORT_H_
#define VM_TOOLS_LAUNCHER_NFS_EXPORT_H_

#include <map>
#include <memory>
#include <string>

#include <brillo/process.h>

#include "vm_tools/launcher/subnet.h"

namespace vm_tools {
namespace launcher {

// Manages an NFS export ID.
// For multiple VMs, the config file needs to be updated with new exports, and
// the config reloaded.
class NfsExport : public PooledResource {
 public:
  ~NfsExport() override;

  unsigned int GetExportID() const;
  const base::FilePath& GetExportPath() const;

  static std::unique_ptr<NfsExport> Create(
      const base::FilePath& instance_runtime_dir,
      const base::FilePath& export_path,
      const std::shared_ptr<Subnet>& subnet);
  static std::unique_ptr<NfsExport> Load(
      const base::FilePath& instance_runtime_dir,
      const std::shared_ptr<Subnet>& subnet);

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
  NfsExport(const base::FilePath& instance_runtime_dir,
            const base::FilePath& export_path,
            const std::shared_ptr<Subnet>& subnet,
            bool release_on_destruction);

  bool IsExportIdAllocated(const unsigned int export_id) const;
  // Writes the configuration file for the nfs server.
  bool ConfigureGanesha();

  // Launch the nfs server.
  bool StartGanesha();

  // Terminates the running nfs server.
  bool StopGanesha();

  // Reload ganesha config.
  bool ReloadGanesha();

  std::map<unsigned int, base::FilePath> allocated_exports_;
  unsigned int export_id_;
  base::FilePath export_path_;
  std::shared_ptr<Subnet> subnet_;

  DISALLOW_COPY_AND_ASSIGN(NfsExport);
};
}  // namespace launcher
}  // namespace vm_tools

#endif  // VM_TOOLS_LAUNCHER_NFS_EXPORT_H_
