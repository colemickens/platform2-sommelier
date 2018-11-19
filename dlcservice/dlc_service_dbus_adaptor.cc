// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/dlc_service_dbus_adaptor.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <brillo/errors/error.h>
#include <brillo/errors/error_codes.h>
#include <chromeos/dbus/service_constants.h>
#include <dlcservice/proto_bindings/dlcservice.pb.h>
#include <update_engine/dbus-constants.h>
#include <update_engine/proto_bindings/update_engine.pb.h>

#include "dlcservice/boot_slot.h"
#include "dlcservice/utils.h"

namespace dlcservice {

namespace {

// Sets the D-Bus error object and logs the error message.
void LogAndSetError(brillo::ErrorPtr* err, const std::string& msg) {
  if (err)
    *err = brillo::Error::Create(FROM_HERE, "dlcservice", "INTERNAL", msg);
  LOG(ERROR) << msg;
}

// The time interval we check for update_engine's progress.
constexpr base::TimeDelta kCheckInterval = base::TimeDelta::FromSeconds(1);
// The retry times to get update_engine's progress before giving up.
constexpr int kRetryCount = 10;

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
                                    const std::string& id_in,
                                    std::string* mount_point_out) {
  // TODO(xiaochu): change API to accept a list of DLC module ids.
  // https://crbug.com/905075

  // Initialize supported DLC module id list.
  std::vector<std::string> dlc_module_ids = ScanDlcModules();
  if (std::find(dlc_module_ids.begin(), dlc_module_ids.end(), id_in) ==
      dlc_module_ids.end()) {
    LogAndSetError(err, "The DLC ID provided is invalid.");
    return false;
  }

  // TODO(xiaochu): may detect corrupted DLC modules and recover.
  // https://crbug.com/903432
  if (base::PathExists(utils::GetDlcModulePath(content_dir_, id_in))) {
    LogAndSetError(err, "The DLC module is installed.");
    return false;
  }

  // Creates DLC module storage.
  base::File::Error file_err;

  // Creates image A.
  base::FilePath image_a_path =
      utils::GetDlcModuleImagePath(content_dir_, id_in, 0);
  if (!base::CreateDirectoryAndGetError(image_a_path.DirName(), &file_err)) {
    std::string error_str = "Failed to create slot A DLC directory: " +
                            base::File::ErrorToString(file_err);
    LogAndSetError(err, error_str);
    return false;
  }
  base::File image_a(image_a_path, base::File::FLAG_CREATE |
                                       base::File::FLAG_READ |
                                       base::File::FLAG_WRITE);
  if (!image_a.IsValid()) {
    std::string error_str = "Failed to create slot A DLC image file: " +
                            base::File::ErrorToString(image_a.error_details());
    LogAndSetError(err, error_str);
    return false;
  }

  // Creates image B.
  base::FilePath image_b_path =
      utils::GetDlcModuleImagePath(content_dir_, id_in, 1);
  if (!base::CreateDirectoryAndGetError(image_b_path.DirName(), &file_err)) {
    std::string error_str = "Failed to create slot B DLC directory: " +
                            base::File::ErrorToString(file_err);
    LogAndSetError(err, error_str);
    return false;
  }
  base::File image_b(image_b_path, base::File::FLAG_CREATE |
                                       base::File::FLAG_READ |
                                       base::File::FLAG_WRITE);
  if (!image_b.IsValid()) {
    std::string error_str = "Failed to create slot B DLC image file: " +
                            base::File::ErrorToString(image_b.error_details());
    LogAndSetError(err, error_str);
    return false;
  }

  // TODO(xiaochu): parse DLC manifest file to set image size.
  // https://crbug.com/904539
  // Currently we set the image file to be 1000K for all DLC modules.
  // This is a reasonable size for test DLC modules for demo purpose.
  int image_size = 1024 * 1000;
  if (!image_a.SetLength(image_size) || !image_b.SetLength(image_size)) {
    LogAndSetError(err,
                   "Failed to reserve backup files for DLC module images.");
    return false;
  }

  if (!CheckForUpdateEngineStatus({update_engine::kUpdateStatusIdle})) {
    LogAndSetError(
        err, "Update Engine is performing operations or a reboot is pending.");
    return false;
  }

  // Invokes update_engine to install the DLC module.
  chromeos_update_engine::DlcParameters dlc_parameters;
  chromeos_update_engine::DlcInfo* dlc_info = dlc_parameters.add_dlc_infos();
  dlc_info->set_dlc_id(id_in);
  std::string dlc_request;
  if (!dlc_parameters.SerializeToString(&dlc_request)) {
    LogAndSetError(err, "protobuf can not be serialized.");
    return false;
  }
  if (!update_engine_proxy_->AttemptInstall(dlc_request, nullptr)) {
    LogAndSetError(err, "Update Engine failed to schedule install operations.");
    return false;
  }

  // TODO(xiaochu): make the API asynchronous.
  // https://crbug.com/905071
  // Code below this point should be moved to a callback (triggered after
  // update_engine finishes install) and the function returns true immediately
  // to unblock the caller. Currently, we make the assumption that a DLC install
  // completes in 10 seconds which is more than enough for downloading (reading)
  // a local file on DUT.
  if (!WaitForUpdateEngineIdle()) {
    LogAndSetError(err, "Failed waiting for update_engine to become idle");
    return false;
  }

  // Mount the installed DLC module image.
  std::string boot_disk_name;
  int num_slots = -1;
  int current_slot = -1;
  if (!boot_slot_->GetCurrentSlot(&boot_disk_name, &num_slots, &current_slot)) {
    LogAndSetError(err, "Can not get current boot slot.");
    return false;
  }
  if (!image_loader_proxy_->LoadDlcImage(
          id_in,
          current_slot == 0 ? imageloader::kSlotNameA : imageloader::kSlotNameB,
          mount_point_out, nullptr)) {
    LogAndSetError(err, "Imageloader is not available.");
    return false;
  }
  if (mount_point_out->empty()) {
    LogAndSetError(err, "Imageloader LoadDlcImage failed.");
    return false;
  }
  return true;
}

bool DlcServiceDBusAdaptor::Uninstall(brillo::ErrorPtr* err,
                                      const std::string& id_in) {
  // Initialize supported DLC module id list.
  std::vector<std::string> dlc_module_ids = ScanDlcModules();

  if (std::find(dlc_module_ids.begin(), dlc_module_ids.end(), id_in) ==
      dlc_module_ids.end()) {
    LogAndSetError(err, "The DLC ID provided is invalid.");
    return false;
  }

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

  if (!CheckForUpdateEngineStatus(
          {update_engine::kUpdateStatusIdle,
           update_engine::kUpdateStatusUpdatedNeedReboot})) {
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
  int num_slots = -1;
  int current_slot = -1;
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
        dlc_module_id,
        current_slot == 0 ? imageloader::kSlotNameA : imageloader::kSlotNameB,
        &path, nullptr);
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

bool DlcServiceDBusAdaptor::WaitForUpdateEngineIdle() {
  for (int i = 0; i < kRetryCount; i++) {
    if (CheckForUpdateEngineStatus({update_engine::kUpdateStatusIdle})) {
      return true;
    }
    base::PlatformThread::Sleep(kCheckInterval);
  }
  return false;
}

bool DlcServiceDBusAdaptor::CheckForUpdateEngineStatus(
    const std::vector<std::string>& status_list) {
  int64_t last_checked_time = 0;
  double progress = 0;
  std::string current_operation;
  std::string new_version;
  int64_t new_size = 0;
  if (!update_engine_proxy_->GetStatus(&last_checked_time, &progress,
                                       &current_operation, &new_version,
                                       &new_size, nullptr)) {
    LOG(ERROR) << "Update Engine is not available.";
    return false;
  }
  if (!std::any_of(status_list.begin(), status_list.end(),
                   [&current_operation](const std::string& status) {
                     return current_operation == status;
                   })) {
    return false;
  }
  return true;
}

}  // namespace dlcservice
