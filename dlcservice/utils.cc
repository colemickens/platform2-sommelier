// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/utils.h"

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/logging.h>

using base::FilePath;
using std::set;
using std::string;

namespace {

constexpr char kDlcDirAName[] = "dlc_a";
constexpr char kDlcDirBName[] = "dlc_b";
constexpr char kDlcImageFileName[] = "dlc.img";
constexpr char kManifestName[] = "imageloader.json";

// The directory inside a DLC module that contains all the DLC files.
constexpr char kRootDirectoryInsideDlcModule[] = "root";

}  // namespace

namespace dlcservice {

namespace utils {

FilePath GetDlcModulePath(const FilePath& dlc_module_root_path,
                          const string& id) {
  return dlc_module_root_path.Append(id);
}

FilePath GetDlcModulePackagePath(const FilePath& dlc_module_root_path,
                                 const string& id,
                                 const string& package) {
  return GetDlcModulePath(dlc_module_root_path, id).Append(package);
}

FilePath GetDlcModuleImagePath(const FilePath& dlc_module_root_path,
                               const string& id,
                               const string& package,
                               int current_slot) {
  if (current_slot < 0) {
    LOG(ERROR) << "current_slot is negative:" << current_slot;
    return FilePath();
  }
  return GetDlcModulePackagePath(dlc_module_root_path, id, package)
      .Append(current_slot == 0 ? kDlcDirAName : kDlcDirBName)
      .Append(kDlcImageFileName);
}

// Extract details about a DLC module from its manifest file.
bool GetDlcManifest(const FilePath& dlc_manifest_path,
                    const string& id,
                    const string& package,
                    imageloader::Manifest* manifest_out) {
  string dlc_json_str;
  FilePath dlc_manifest_file =
      GetDlcModulePackagePath(dlc_manifest_path, id, package)
          .Append(kManifestName);

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

FilePath GetDlcRootInModulePath(const FilePath& dlc_mount_point) {
  return dlc_mount_point.Append(kRootDirectoryInsideDlcModule);
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

}  // namespace utils

}  // namespace dlcservice
