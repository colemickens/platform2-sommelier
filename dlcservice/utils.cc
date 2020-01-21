// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/utils.h"

#include <utility>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/logging.h>

#include "dlcservice/dlc_service.h"

using base::FilePath;
using std::pair;
using std::set;
using std::string;

namespace dlcservice {

char kDlcDirAName[] = "dlc_a";
char kDlcDirBName[] = "dlc_b";
char kDlcPreloadAllowedName[] = "preload_allowed";

char kDlcImageFileName[] = "dlc.img";
char kManifestName[] = "imageloader.json";

char kRootDirectoryInsideDlcModule[] = "root";

const mode_t kDlcModuleDirectoryPerms = 0755;

bool CreateDirWithDlcPermissions(const base::FilePath& path) {
  base::File::Error file_err;
  if (!base::CreateDirectoryAndGetError(path, &file_err)) {
    LOG(ERROR) << "Failed to create directory '" << path.value()
               << "': " << base::File::ErrorToString(file_err);
    return false;
  }
  if (!base::SetPosixFilePermissions(path, kDlcModuleDirectoryPerms)) {
    LOG(ERROR) << "Failed to set directory permissions for '" << path.value()
               << "'";
    return false;
  }
  return true;
}

bool CreateFile(const base::FilePath& path, int64_t size) {
  if (!CreateDirWithDlcPermissions(path.DirName())) {
    return false;
  }
  base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to create file at " << path.value() << " reason: "
               << base::File::ErrorToString(file.error_details());
    return false;
  }
  if (!file.SetLength(size)) {
    LOG(ERROR) << "Failed to set legnth (" << size << ") for " << path.value();
    return false;
  }
  return true;
}

bool ResizeFile(const base::FilePath& path, int64_t size) {
  base::File f(path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  if (!f.IsValid()) {
    LOG(ERROR) << "Failed to open file to resize '" << path.value()
               << "': " << base::File::ErrorToString(f.error_details());
    return false;
  }
  if (!f.SetLength(size)) {
    PLOG(ERROR) << "Failed to set length (" << size << ") for " << path.value();
    return false;
  }
  return true;
}

bool CopyAndResizeFile(const base::FilePath& from,
                       const base::FilePath& to,
                       int64_t size) {
  if (!base::CopyFile(from, to)) {
    PLOG(ERROR) << "Failed to copy from (" << from.value() << ") to ("
                << to.value() << ").";
    return false;
  }
  ResizeFile(to, size);
  return true;
}

FilePath GetDlcImagePath(const FilePath& dlc_module_root_path,
                         const string& id,
                         const string& package,
                         BootSlot::Slot slot) {
  return JoinPaths(dlc_module_root_path, id, package)
      .Append(slot == BootSlot::Slot::A ? kDlcDirAName : kDlcDirBName)
      .Append(kDlcImageFileName);
}

// Extract details about a DLC module from its manifest file.
bool GetDlcManifest(const FilePath& dlc_manifest_path,
                    const string& id,
                    const string& package,
                    imageloader::Manifest* manifest_out) {
  string dlc_json_str;
  FilePath dlc_manifest_file =
      JoinPaths(dlc_manifest_path, id, package, kManifestName);

  if (!base::ReadFileToString(dlc_manifest_file, &dlc_json_str)) {
    LOG(ERROR) << "Failed to read DLC manifest file '"
               << dlc_manifest_file.value() << "'.";
    return false;
  }

  if (!manifest_out->ParseManifest(dlc_json_str)) {
    LOG(ERROR) << "Failed to parse DLC manifest for DLC:" << id << ".";
    return false;
  }

  return true;
}

FilePath GetDlcRootInModulePath(const FilePath& dlc_mount_point) {
  return JoinPaths(dlc_mount_point, kRootDirectoryInsideDlcModule);
}

set<string> ScanDirectory(const FilePath& dir) {
  set<string> result;
  base::FileEnumerator file_enumerator(dir, false,
                                       base::FileEnumerator::DIRECTORIES);
  for (FilePath dir_path = file_enumerator.Next(); !dir_path.empty();
       dir_path = file_enumerator.Next()) {
    result.emplace(dir_path.BaseName().value());
  }
  return result;
}

DlcModuleList ToDlcModuleList(const DlcRootMap& dlcs,
                              std::function<bool(DlcId, DlcRoot)> filter) {
  DlcModuleList dlc_module_list;
  auto f = [&dlc_module_list, filter](const pair<DlcId, DlcRoot>& pr) {
    if (filter(pr.first, pr.second)) {
      DlcModuleInfo* dlc_module_info = dlc_module_list.add_dlc_module_infos();
      dlc_module_info->set_dlc_id(pr.first);
      dlc_module_info->set_dlc_root(pr.second);
    }
  };
  for_each(begin(dlcs), end(dlcs), f);
  return dlc_module_list;
}

DlcRootMap ToDlcRootMap(const DlcModuleList& dlc_module_list,
                        std::function<bool(DlcModuleInfo)> filter) {
  DlcRootMap m;
  for (const DlcModuleInfo& dlc_module : dlc_module_list.dlc_module_infos()) {
    if (filter(dlc_module))
      m.emplace(dlc_module.dlc_id(), dlc_module.dlc_root());
  }
  return m;
}

dlcservice::InstallStatus CreateInstallStatus(
    const dlcservice::Status& status,
    const std::string& error_code,
    const dlcservice::DlcModuleList& dlc_module_list,
    double progress) {
  InstallStatus install_status;
  install_status.set_status(status);
  install_status.set_error_code(error_code);
  install_status.mutable_dlc_module_list()->CopyFrom(dlc_module_list);
  install_status.set_progress(progress);
  return install_status;
}

}  // namespace dlcservice
