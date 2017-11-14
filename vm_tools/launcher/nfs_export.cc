// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/launcher/nfs_export.h"

#include <vector>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/process/process_iterator.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/stringprintf.h>
#include <brillo/process.h>
#include <brillo/syslog_logging.h>
#include <sys/signal.h>

#include "vm_tools/launcher/constants.h"
#include "vm_tools/launcher/subnet.h"

namespace vm_tools {
namespace launcher {

namespace {

// Path to ganesha's temporary config and log directory.
constexpr char kGaneshaConfigDirectory[] = "/run/ganesha";

// Name of nfs-ganesha's upstart job.
constexpr char kGaneshaJobName[] = "nfs-ganesha";

}  // namespace

bool NfsExport::StartRpcBind() {
  brillo::ProcessImpl rpcbind;

  rpcbind.AddArg("/sbin/start");
  rpcbind.AddArg("rpcbind");

  LOG(INFO) << "Starting rpcbind";

  if (rpcbind.Run() != 0) {
    PLOG(ERROR) << "Unable to start rpcbind";
    return false;
  }

  // Starting rpcbind will automatically start nfs-ganesha due to upstart
  // dependencies.

  return true;
}

bool NfsExport::StopRpcBind() {
  brillo::ProcessImpl rpcbind;
  rpcbind.AddArg("/sbin/stop");
  rpcbind.AddArg("rpcbind");

  LOG(INFO) << "Stopping rpcbind";
  if (rpcbind.Run() != 0) {
    PLOG(ERROR) << "Unable to stop rpcbind";
    return false;
  }

  return true;
}

bool NfsExport::ReloadGanesha() {
  brillo::ProcessImpl nfs_upstart;
  nfs_upstart.AddArg("/sbin/reload");
  nfs_upstart.AddArg(kGaneshaJobName);

  LOG(INFO) << "Reloading NFS config";
  if (nfs_upstart.Run() != 0) {
    PLOG(ERROR) << "Unable to reload NFS config";
    return false;
  }
  return true;
}

bool NfsExport::ConfigureGanesha() {
  std::string config = R"XXX(
NFS_Core_Param {
    MNT_Port = 2050;
}
)XXX";

  constexpr char export_template[] = R"XXX(
EXPORT
{
  Export_Id = %u;
  # Minijail-relative path.
  Path = %s;
  Squash = Root;
  # chronos uid/gid.
  Anonymous_Uid = 1000;
  Anonymous_Gid = 1000;
  Access_Type = NONE;
  Protocols = 3;
  FSAL {
    Name = VFS;
  }
  CLIENT {
    Clients = %s;
    Access_Type = RW;
  }
}
)XXX";

  base::FilePath config_directory(launcher::kGaneshaConfigDirectory);
  if (!base::DirectoryExists(config_directory)) {
    LOG(INFO) << "Config directory " << launcher::kGaneshaConfigDirectory
              << " does not exist, creating.";
    if (!base::CreateDirectory(config_directory)) {
      PLOG(ERROR) << "Unable to create config directory";
      return false;
    }
  }
  base::FilePath config_file_path =
      base::FilePath(launcher::kGaneshaConfigDirectory)
          .Append(base::FilePath("ganesha.conf"));

  // Assemble ganesha config.
  for (const auto& pair : allocated_exports_) {
    base::StringAppendF(&config, export_template, pair.first,
                        pair.second.value().c_str(),
                        subnet_->GetIpAddress().c_str());
  }

  int write_len =
      base::WriteFile(config_file_path, config.c_str(), config.length());
  if (write_len != config.length()) {
    PLOG(ERROR) << "Unable to write config file " << config_file_path.value();
    return false;
  }

  return true;
}

unsigned int NfsExport::GetExportID() const {
  return export_id_;
}

const base::FilePath& NfsExport::GetExportPath() const {
  return export_path_;
}

NfsExport::NfsExport(const base::FilePath& instance_runtime_dir,
                     const base::FilePath& export_path,
                     const std::shared_ptr<Subnet>& subnet,
                     bool release_on_destruction)
    : PooledResource(instance_runtime_dir, release_on_destruction),
      export_path_(export_path),
      subnet_(subnet) {}

NfsExport::~NfsExport() {
  if (ShouldReleaseOnDestruction() && !Release())
    LOG(ERROR) << "Failed to Release() NFS export ID";
}

std::unique_ptr<NfsExport> NfsExport::Create(
    const base::FilePath& instance_runtime_dir,
    const base::FilePath& export_path,
    const std::shared_ptr<Subnet>& subnet) {
  auto nfs_export = std::unique_ptr<NfsExport>(
      new NfsExport(instance_runtime_dir, export_path, subnet, true));
  if (!nfs_export->Allocate())
    return nullptr;

  return nfs_export;
}

std::unique_ptr<NfsExport> NfsExport::Load(
    const base::FilePath& instance_runtime_dir,
    const std::shared_ptr<Subnet>& subnet) {
  auto nfs_export = std::unique_ptr<NfsExport>(
      new NfsExport(instance_runtime_dir, base::FilePath(), subnet, false));
  if (!nfs_export->LoadInstance())
    return nullptr;

  return nfs_export;
}

const char* NfsExport::GetName() const {
  return "nfs_export";
}

const std::string NfsExport::GetResourceID() const {
  return base::StringPrintf("%u", export_id_);
}

bool NfsExport::LoadGlobalResources(const std::string& resources) {
  allocated_exports_.clear();

  for (const auto& line : base::SplitStringPiece(
           resources, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    unsigned int id;
    std::vector<std::string> parts = base::SplitString(
        line, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    if (parts.size() != 2) {
      LOG(ERROR) << "Failed to parse export ID/path";
      allocated_exports_.clear();
      return false;
    }

    if (!base::StringToUint(parts[0], &id)) {
      LOG(ERROR) << "Failed to read export ID";
      allocated_exports_.clear();
      return false;
    }
    if (allocated_exports_.find(id) != allocated_exports_.end())
      LOG(WARNING) << "Export " << id << " was used twice";

    base::FilePath export_path(parts[1]);
    if (!base::DirectoryExists(export_path)) {
      LOG(ERROR) << "Export path doesn't exist: " << export_path.value();
      allocated_exports_.clear();
      return false;
    }

    allocated_exports_[id] = export_path;
  }

  return true;
}

std::string NfsExport::PersistGlobalResources() {
  std::string resources;
  for (const auto& pair : allocated_exports_) {
    base::StringAppendF(&resources, "%u:%s\n", pair.first,
                        pair.second.value().c_str());
  }

  return resources;
}

bool NfsExport::LoadInstanceResource(const std::string& resource) {
  unsigned int id;

  if (!base::StringToUint(resource, &id))
    return false;

  if (!IsExportIdAllocated(id))
    return false;

  export_id_ = id;
  export_path_ = allocated_exports_[id];
  return true;
}

bool NfsExport::AllocateResource() {
  unsigned int export_id = 1;

  // std::map rbegin is the largest value in the map. Increment this by one
  // to get our next export id.
  auto it = allocated_exports_.rbegin();
  if (it != allocated_exports_.rend())
    export_id = it->first + 1;

  export_id_ = export_id;
  allocated_exports_[export_id_] = export_path_;

  if (!ConfigureGanesha())
    return false;

  if (allocated_exports_.size() == 1) {
    if (!StartRpcBind())
      return false;
  } else {
    if (!ReloadGanesha())
      return false;
  }

  return true;
}

bool NfsExport::ReleaseResource() {
  allocated_exports_.erase(export_id_);

  if (!ConfigureGanesha())
    return false;

  if (allocated_exports_.empty()) {
    if (!StopRpcBind())
      return false;
  } else {
    if (!ReloadGanesha())
      return false;
  }

  return true;
}

bool NfsExport::IsExportIdAllocated(const unsigned int export_id) const {
  return allocated_exports_.find(export_id) != allocated_exports_.end();
}

}  // namespace launcher
}  // namespace vm_tools
