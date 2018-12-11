// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/utils.h"

#include <base/files/file_util.h>
#include <base/logging.h>

namespace {
constexpr char kDlcDirAName[] = "dlc_a";
constexpr char kDlcDirBName[] = "dlc_b";
constexpr char kDlcImageFileName[] = "dlc.img";
constexpr char kManifestName[] = "imageloader.json";
}  // namespace

namespace dlcservice {

constexpr char kManifestDir[] = "/opt/google/dlc";

namespace utils {

base::FilePath GetDlcModulePath(const base::FilePath& dlc_module_root_path,
                                const std::string& dlc_module_id) {
  return dlc_module_root_path.Append(dlc_module_id);
}

base::FilePath GetDlcModuleImagePath(const base::FilePath& dlc_module_root_path,
                                     const std::string& dlc_module_id,
                                     int current_slot) {
  if (current_slot < 0) {
    LOG(ERROR) << "current_slot is negative:" << current_slot;
    return base::FilePath();
  }
  return GetDlcModulePath(dlc_module_root_path, dlc_module_id)
      .Append(current_slot == 0 ? kDlcDirAName : kDlcDirBName)
      .Append(kDlcImageFileName);
}

// Extract details about a DLC module from its manifest file.
bool GetDlcManifest(const std::string& dlc_module_id,
                    imageloader::Manifest* manifest_out) {
  std::string dlc_json_str;
  base::FilePath dlc_manifest_path(kManifestDir);
  base::FilePath dlc_manifest_file =
      dlc_manifest_path.Append(dlc_module_id).Append(kManifestName);

  if (!base::ReadFileToString(dlc_manifest_file, &dlc_json_str)) {
    LOG(ERROR) << "Failed to read DLC manifest file '"
               << dlc_manifest_file.value() << "'.";
    return false;
  }

  if (!manifest_out->ParseManifest(dlc_json_str)) {
    LOG(ERROR) << "Failed to parse DLC manifest.";
    return false;
  }

  return true;
}

}  // namespace utils

}  // namespace dlcservice
