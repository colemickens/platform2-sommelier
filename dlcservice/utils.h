// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DLCSERVICE_UTILS_H_
#define DLCSERVICE_UTILS_H_

#include <set>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <libimageloader/manifest.h>

namespace dlcservice {

namespace utils {

template <typename BindedCallback>
class ScopedCleanups {
 public:
  ScopedCleanups() = default;
  ~ScopedCleanups() {
    for (const auto& cleanup : queue_)
      cleanup.Run();
  }
  void Insert(BindedCallback cleanup) { queue_.push_back(cleanup); }
  // Clears everything so destructor is a no-op.
  void Cancel() { queue_.clear(); }

 private:
  std::vector<BindedCallback> queue_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCleanups<BindedCallback>);
};

// Returns the path to a DLC module ID's based directory given |id|.
base::FilePath GetDlcModulePath(const base::FilePath& dlc_module_root_path,
                                const std::string& id);

// Returns the path to a DLC module base directory given the |id| and |package|.
base::FilePath GetDlcModulePackagePath(
    const base::FilePath& dlc_module_root_path,
    const std::string& id,
    const std::string& package);

// Returns the path to a DLC module image given the |id| and |package|.
base::FilePath GetDlcModuleImagePath(const base::FilePath& dlc_module_root_path,
                                     const std::string& id,
                                     const std::string& package,
                                     int current_slot);

bool GetDlcManifest(const base::FilePath& dlc_manifest_path,
                    const std::string& id,
                    const std::string& package,
                    imageloader::Manifest* manifest_out);

// Returns the directory inside a DLC module which is mounted at
// |dlc_mount_point|.
base::FilePath GetDlcRootInModulePath(const base::FilePath& dlc_mount_point);

// Scans a directory and returns all its subdirectory names in a list.
std::set<std::string> ScanDirectory(const base::FilePath& dir);

}  // namespace utils
}  // namespace dlcservice

#endif  // DLCSERVICE_UTILS_H_
