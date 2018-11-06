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
#include <update_engine/dbus-constants.h>

#include "dlcservice/boot_slot.h"
#include "dlcservice/utils.h"

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
  LogAndSetError(err, "This API is not implemented.");
  return false;
}

bool DlcServiceDBusAdaptor::Uninstall(brillo::ErrorPtr* err,
                                      const std::string& id_in) {
  // Initialize supported DLC module id list.
  std::vector<std::string> dlc_module_ids = ScanDlcModules();
  // Checks if the DLC module id is valid.
  if (std::find(dlc_module_ids.begin(), dlc_module_ids.end(), id_in) ==
      dlc_module_ids.end()) {
    LogAndSetError(err, "The DLC ID provided is invalid.");
    return false;
  }
  // Checks if the DLC module is installed.
  const base::FilePath dlc_module_content_path =
      utils::GetDlcModulePath(content_dir_, id_in);
  if (!base::PathExists(dlc_module_content_path) ||
      !base::PathExists(utils::GetDlcModuleImagePath(content_dir_, id_in, 0)) ||
      !base::PathExists(utils::GetDlcModuleImagePath(content_dir_, id_in, 1))) {
    LogAndSetError(err, "The DLC module is not installed properly.");
    return false;
  }
  // Unmounts the DLC module image.
  bool success = false;
  if (!image_loader_proxy_->UnloadDlcImage(id_in, &success, nullptr)) {
    LogAndSetError(err, "Imageloader is not available.");
    return false;
  }
  if (!success) {
    LogAndSetError(err, "Imageloader UnloadDlcImage failed.");
    return false;
  }
  // Checks whether update_engine is busy.
  int64_t last_checked_time = 0;
  double progress = 0;
  std::string current_operation;
  std::string new_version;
  int64_t new_size = 0;
  if (!update_engine_proxy_->GetStatus(&last_checked_time, &progress,
                                       &current_operation, &new_version,
                                       &new_size, nullptr)) {
    LogAndSetError(err, "Update Engine is not available.");
    return false;
  }
  if (current_operation != update_engine::kUpdateStatusIdle &&
      current_operation != update_engine::kUpdateStatusUpdatedNeedReboot) {
    LogAndSetError(err, "Update Engine is performing operations.");
    return false;
  }
  // Deletes the DLC module images.
  if (!base::DeleteFile(dlc_module_content_path, true)) {
    LogAndSetError(err, "DLC image folder could not be deleted.");
    return false;
  }
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
        utils::GetDlcModulePath(content_dir_, dlc_module_id);
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
  int current_slot;
  if (!boot_slot_->GetCurrentSlot(&boot_disk_name, &num_slots, &current_slot)) {
    LOG(ERROR) << "Can not get current boot slot.";
    return;
  }
  // Load all installed DLC modules.
  for (const auto& dlc_module_id : dlc_module_ids) {
    auto dlc_module_content_path =
        utils::GetDlcModulePath(content_dir_, dlc_module_id);
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
