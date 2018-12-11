// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DLCSERVICE_UTILS_H_
#define DLCSERVICE_UTILS_H_

#include <string>

#include <base/files/file_path.h>
#include <libimageloader/manifest.h>

namespace dlcservice {

// The root dir that stores all available DLC manifests.
extern const char kManifestDir[];

namespace utils {

// Returns the path to a DLC module.
base::FilePath GetDlcModulePath(const base::FilePath& dlc_module_root_path,
                                const std::string& dlc_module_id);

// Returns the path to a DLC module image.
base::FilePath GetDlcModuleImagePath(const base::FilePath& dlc_module_root_path,
                                     const std::string& dlc_module_id,
                                     int current_slot);

bool GetDlcManifest(const std::string& dlc_module_id,
                    imageloader::Manifest* manifest_out);

}  // namespace utils
}  // namespace dlcservice

#endif  // DLCSERVICE_UTILS_H_
