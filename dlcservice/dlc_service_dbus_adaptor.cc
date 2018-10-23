// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/dlc_service_dbus_adaptor.h"

#include <utility>
#include <vector>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <brillo/errors/error.h>
#include <brillo/errors/error_codes.h>
#include <dlcservice/proto_bindings/dlcservice.pb.h>

#include "dlcservice/boot_slot.h"

namespace dlcservice {

namespace {

// Slot names (A or B) for DLC module images.
constexpr char kSlotNameA[] = "Dlc-A";
constexpr char kSlotNameB[] = "Dlc-B";

// Sets the D-Bus error object and logs the error message.
void LogAndSetError(brillo::ErrorPtr* err, const std::string& msg) {
  if (err)
    *err = brillo::Error::Create(FROM_HERE, "dlcservice", "INTERNAL", msg);
  LOG(ERROR) << msg;
}

// Returns the path to a DLC module.
base::FilePath GetDlcModulePath(const base::FilePath& dlc_module_root_path,
                                const std::string& dlc_module_id) {
  return dlc_module_root_path.Append(dlc_module_id);
}

}  // namespace

DlcServiceDBusAdaptor::DlcServiceDBusAdaptor(
    std::unique_ptr<org::chromium::ImageLoaderInterfaceProxyInterface>
        image_loader_proxy,
    std::unique_ptr<org::chromium::UpdateEngineInterfaceProxyInterface>
        update_engine_proxy,
    std::unique_ptr<BootSlot> boot_slot,
    const base::FilePath& manifest_dir,
    const base::FilePath& content_dir)
    : org::chromium::DlcServiceInterfaceAdaptor(this),
      image_loader_proxy_(std::move(image_loader_proxy)),
      update_engine_proxy_(std::move(update_engine_proxy)),
      boot_slot_(std::move(boot_slot)),
      manifest_dir_(manifest_dir),
      content_dir_(content_dir) {
  // TODO(xiaochu): Allows dlcservice being activated on-demand.
  // https://crbug.com/898255
  LoadDlcModuleImages();
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

bool DlcServiceDBusAdaptor::GetInstalled(brillo::ErrorPtr* err,
                                         std::string* dlc_module_list_out) {
  // Initialize supported DLC module id list.
  std::vector<std::string> dlc_module_ids = ScanDlcModules();

  // Find installed DLC modules.
  DlcModuleList dlc_module_list;
  for (const auto& dlc_module_id : dlc_module_ids) {
    auto dlc_module_content_path =
        GetDlcModulePath(content_dir_, dlc_module_id);
    if (base::PathExists(dlc_module_content_path)) {
      DlcModuleInfo* dlc_module_info = dlc_module_list.add_dlc_module_infos();
      dlc_module_info->set_dlc_id(dlc_module_id);
    }
  }
  if (!dlc_module_list.SerializeToString(dlc_module_list_out)) {
    LogAndSetError(err, "Failed to serialize the protobuf.");
    return false;
  }
  return true;
}

void DlcServiceDBusAdaptor::LoadDlcModuleImages() {
  // Initialize supported DLC module id list.
  std::vector<std::string> dlc_module_ids = ScanDlcModules();

  std::string boot_disk_name;
  int num_slots;
  unsigned int current_slot;
  if (!boot_slot_->GetCurrentSlot(&boot_disk_name, &num_slots, &current_slot)) {
    LOG(ERROR) << "Can not get current boot slot.";
    return;
  }
  // Load all installed DLC modules.
  for (const auto& dlc_module_id : dlc_module_ids) {
    auto dlc_module_content_path =
        GetDlcModulePath(content_dir_, dlc_module_id);
    if (!base::PathExists(dlc_module_content_path))
      continue;
    // Mount the installed DLC image.
    std::string path;
    image_loader_proxy_->LoadDlcImage(
        dlc_module_id, current_slot == 0 ? kSlotNameA : kSlotNameB, &path,
        nullptr);
    if (path.empty()) {
      LOG(ERROR) << "DLC image " << dlc_module_id << " is corrupted.";
    } else {
      LOG(INFO) << "DLC image " << dlc_module_id << " is mounted at " << path;
    }
  }
}

std::vector<std::string> DlcServiceDBusAdaptor::ScanDlcModules() {
  std::vector<std::string> dlc_module_ids;
  base::FileEnumerator file_enumerator(manifest_dir_, false,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath dir_path = file_enumerator.Next(); !dir_path.empty();
       dir_path = file_enumerator.Next()) {
    dlc_module_ids.emplace_back(dir_path.BaseName().value());
  }
  return dlc_module_ids;
}

}  // namespace dlcservice
