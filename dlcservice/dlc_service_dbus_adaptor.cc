// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/dlc_service_dbus_adaptor.h"

#include <utility>
#include <vector>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>

namespace dlcservice {

DlcServiceDBusAdaptor::DlcServiceDBusAdaptor(
    std::unique_ptr<org::chromium::ImageLoaderInterfaceProxyInterface>
        image_loader_proxy,
    const base::FilePath& manifest_dir,
    const base::FilePath& content_dir)
    : org::chromium::DlcServiceInterfaceAdaptor(this),
      image_loader_proxy_(std::move(image_loader_proxy)),
      manifest_dir_(manifest_dir),
      content_dir_(content_dir) {
  LoadDlcImages();
}

DlcServiceDBusAdaptor::~DlcServiceDBusAdaptor() {}

bool DlcServiceDBusAdaptor::Install(brillo::ErrorPtr* err,
                                    const std::string& in_id,
                                    std::string* mount_point_out) {
  // TODO(xiaochu): implement this.
  *mount_point_out = "";
  return true;
}

bool DlcServiceDBusAdaptor::Uninstall(brillo::ErrorPtr* err,
                                      const std::string& id_in) {
  // TODO(xiaochu): implement this.
  return true;
}

void DlcServiceDBusAdaptor::LoadDlcImages() {
  // Initialize available DLC list.
  std::vector<std::string> dlc_ids;
  base::FileEnumerator file_enumerator(manifest_dir_, false,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath dir_path = file_enumerator.Next(); !dir_path.empty();
       dir_path = file_enumerator.Next()) {
    dlc_ids.emplace_back(dir_path.BaseName().value());
  }

  // Initialize the states of installed DLC.
  for (auto& dlc_id : dlc_ids) {
    auto image_path = content_dir_.Append(dlc_id);
    if (base::PathExists(image_path)) {
      // Mount the installed DLC image.
      std::string path;
      image_loader_proxy_->LoadDlcImage(dlc_id, "Dlc-A", &path, nullptr);
      if (path.empty()) {
        LOG(ERROR) << "DLC image " << dlc_id << " is corrupted.";
      } else {
        LOG(INFO) << "DLC image " << dlc_id << " is mounted at " << path;
      }
    }
  }
}

}  // namespace dlcservice
