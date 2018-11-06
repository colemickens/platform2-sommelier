// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/utils.h"

namespace dlcservice {

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
      .Append(current_slot == 0 ? "dlc_a" : "dlc_b")
      .Append("dlc.img");
}

}  // namespace utils

}  // namespace dlcservice
