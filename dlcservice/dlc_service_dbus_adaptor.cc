// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/dlc_service_dbus_adaptor.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/message_loop/message_loop.h>
#include <brillo/errors/error.h>
#include <brillo/errors/error_codes.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/dlcservice/dbus-constants.h>
#include <update_engine/dbus-constants.h>

#include "dlcservice/boot_slot.h"
#include "dlcservice/utils.h"

using base::Callback;
using base::File;
using base::FilePath;
using base::ScopedTempDir;
using std::string;
using std::unique_ptr;
using std::vector;
using update_engine::Operation;
using update_engine::StatusResult;

namespace dlcservice {

namespace {

// Permissions for DLC module directories.
constexpr mode_t kDlcModuleDirectoryPerms = 0755;

// Creates a directory with permissions required for DLC modules.
bool CreateDirWithDlcPermissions(const FilePath& path) {
  File::Error file_err;
  if (!base::CreateDirectoryAndGetError(path, &file_err)) {
    LOG(ERROR) << "Failed to create directory '" << path.value()
               << "': " << File::ErrorToString(file_err);
    return false;
  }
  if (!base::SetPosixFilePermissions(path, kDlcModuleDirectoryPerms)) {
    LOG(ERROR) << "Failed to set directory permissions for '" << path.value()
               << "'";
    return false;
  }
  return true;
}

// Creates a directory with an empty image file and resizes it to the given
// size.
bool CreateImageFile(const FilePath& path, int64_t image_size) {
  if (!CreateDirWithDlcPermissions(path.DirName())) {
    return false;
  }
  constexpr uint32_t file_flags =
      File::FLAG_CREATE | File::FLAG_READ | File::FLAG_WRITE;
  File file(path, file_flags);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to create image file '" << path.value()
               << "': " << File::ErrorToString(file.error_details());
    return false;
  }
  if (!file.SetLength(image_size)) {
    LOG(ERROR) << "Failed to reserve backup file for DLC module image '"
               << path.value() << "'";
    return false;
  }
  return true;
}

// Sets the D-Bus error object and logs the error message.
void LogAndSetError(brillo::ErrorPtr* err, const string& msg) {
  if (err)
    *err = brillo::Error::Create(FROM_HERE, "dlcservice", "INTERNAL", msg);
  LOG(ERROR) << msg;
}

}  // namespace

DlcServiceDBusAdaptor::DlcServiceDBusAdaptor(
    unique_ptr<org::chromium::ImageLoaderInterfaceProxyInterface>
        image_loader_proxy,
    unique_ptr<org::chromium::UpdateEngineInterfaceProxyInterface>
        update_engine_proxy,
    unique_ptr<BootSlot> boot_slot,
    const FilePath& manifest_dir,
    const FilePath& content_dir)
    : org::chromium::DlcServiceInterfaceAdaptor(this),
      image_loader_proxy_(std::move(image_loader_proxy)),
      update_engine_proxy_(std::move(update_engine_proxy)),
      boot_slot_(std::move(boot_slot)),
      manifest_dir_(manifest_dir),
      content_dir_(content_dir),
      weak_ptr_factory_(this) {
  // Get current boot slot.
  string boot_disk_name;
  int num_slots = -1;
  int current_boot_slot = -1;
  if (!boot_slot_->GetCurrentSlot(&boot_disk_name, &num_slots,
                                  &current_boot_slot))
    LOG(FATAL) << "Can not get current boot slot.";

  current_boot_slot_name_ = current_boot_slot == 0 ? imageloader::kSlotNameA
                                                   : imageloader::kSlotNameB;

  // Register D-Bus signal callbacks.
  update_engine_proxy_->RegisterStatusUpdateAdvancedSignalHandler(
      base::Bind(&DlcServiceDBusAdaptor::OnStatusUpdateAdvancedSignal,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&DlcServiceDBusAdaptor::OnStatusUpdateAdvancedSignalConnected,
                 weak_ptr_factory_.GetWeakPtr()));

  // Initalize installed DLC modules.
  installed_dlc_modules_ = utils::ScanDirectory(content_dir_);

  // Initialize supported DLC modules.
  supported_dlc_modules_ = utils::ScanDirectory(manifest_dir_);
}

DlcServiceDBusAdaptor::~DlcServiceDBusAdaptor() {}

void DlcServiceDBusAdaptor::LoadDlcModuleImages() {
  // Load all installed DLC modules.
  for (auto installed_dlc_module_itr = installed_dlc_modules_.begin();
       installed_dlc_module_itr != installed_dlc_modules_.end();
       /* Don't increment here */) {
    string installed_dlc_module_id = *installed_dlc_module_itr;

    // TODO(crbug.com/990449): Support restart of dlcservice to handle
    // remounting or getting old mount point back to get into a valid state.

    string mount_point;
    if (!MountDlc(nullptr, installed_dlc_module_id, &mount_point)) {
      LOG(ERROR) << "Failed to mount DLC module during load: "
                 << installed_dlc_module_id;
      if (!DeleteDlc(nullptr, installed_dlc_module_id)) {
        LOG(ERROR) << "Failed to delete an unmountable DLC module: "
                   << installed_dlc_module_id;
      }
      installed_dlc_modules_.erase(installed_dlc_module_itr++);
    } else {
      ++installed_dlc_module_itr;
    }
  }
}

bool DlcServiceDBusAdaptor::Install(brillo::ErrorPtr* err,
                                    const DlcModuleList& dlc_module_list_in) {
  const auto& dlc_modules = dlc_module_list_in.dlc_module_infos();
  if (dlc_modules.empty()) {
    LogAndSetError(err, "Must provide at least one DLC to install");
    return false;
  }

  // Note: this holds the list of directories that were created and need to be
  // freed in case an error happens.
  vector<unique_ptr<ScopedTempDir>> scoped_paths;

  for (const DlcModuleInfo& dlc_module : dlc_modules) {
    FilePath path;
    const string& id = dlc_module.dlc_id();
    auto scoped_path = std::make_unique<ScopedTempDir>();

    if (!CreateDlc(err, id, &path))
      return false;

    if (!scoped_path->Set(path)) {
      LOG(ERROR) << "Failed when scoping path during install: " << path.value();
      return false;
    }

    scoped_paths.emplace_back(std::move(scoped_path));
  }

  if (!CheckForUpdateEngineStatus({update_engine::IDLE})) {
    LogAndSetError(
        err, "Update Engine is performing operations or a reboot is pending.");
    return false;
  }

  // Invokes update_engine to install the DLC module.
  if (!update_engine_proxy_->AttemptInstall(dlc_module_list_in, nullptr)) {
    LogAndSetError(err, "Update Engine failed to schedule install operations.");
    return false;
  }

  dlc_modules_being_installed_ = dlc_module_list_in;
  // Note: Do NOT add to installed indication. Let
  // |OnStatusUpdateAdvancedSignal()| handle since hat's truly when the DLC(s)
  // are installed.

  // Safely take ownership of scoped paths for them not to be freed.
  for (const auto& scoped_path : scoped_paths)
    scoped_path->Take();

  return true;
}

// TODO(crbug/986391): Need to take a protobuf as argument and not a single DLC.
bool DlcServiceDBusAdaptor::Uninstall(brillo::ErrorPtr* err,
                                      const string& id_in) {
  if (installed_dlc_modules_.find(id_in) == installed_dlc_modules_.end()) {
    LogAndSetError(err, "The DLC ID provided is not installed");
    return false;
  }

  if (!CheckForUpdateEngineStatus(
          {update_engine::IDLE, update_engine::UPDATED_NEED_REBOOT})) {
    LogAndSetError(err, "Update Engine is performing operations.");
    return false;
  }

  if (!UnmountDlc(err, id_in))
    return false;

  if (!DeleteDlc(err, id_in))
    return false;

  LOG(INFO) << "Uninstalling DLC id:" << id_in;
  installed_dlc_modules_.erase(id_in);
  return true;
}

bool DlcServiceDBusAdaptor::GetInstalled(brillo::ErrorPtr* err,
                                         DlcModuleList* dlc_module_list_out) {
  const auto InsertIntoDlcModuleListOut =
      [dlc_module_list_out](const string& dlc_module_id) {
        DlcModuleInfo* dlc_module_info =
            dlc_module_list_out->add_dlc_module_infos();
        dlc_module_info->set_dlc_id(dlc_module_id);
      };
  for_each(begin(installed_dlc_modules_), end(installed_dlc_modules_),
           InsertIntoDlcModuleListOut);
  return true;
}

bool DlcServiceDBusAdaptor::InstallingComplete(
    const StatusResult& status_result) {
  if (!status_result.is_install()) {
    LOG(INFO) << "Signal from update_engine, not for install.";
    return false;
  }

  if (status_result.current_operation() != Operation::IDLE) {
    LOG(INFO) << "Signal from update_engine, but install not complete.";
    return false;
  }

  if (dlc_modules_being_installed_.dlc_module_infos().empty()) {
    LOG(ERROR) << "Signal from update_engine, but nothing to install";
    return false;
  }

  LOG(INFO)
      << "Signal from update_engine, proceeding to complete installation.";
  return true;
}

bool DlcServiceDBusAdaptor::CreateDlc(brillo::ErrorPtr* err,
                                      const string& id,
                                      FilePath* path) {
  path->clear();
  if (supported_dlc_modules_.find(id) == supported_dlc_modules_.end()) {
    LogAndSetError(err, "The DLC ID provided is not supported.");
    return false;
  }

  const string& package = ScanDlcModulePackage(id);
  FilePath module_path = utils::GetDlcModulePath(content_dir_, id);
  FilePath module_package_path =
      utils::GetDlcModulePackagePath(content_dir_, id, package);

  if (base::PathExists(module_path)) {
    LogAndSetError(err, "The DLC module is installed or duplicate.");
    return false;
  }
  // Create the DLC ID directory with correct permissions.
  if (!CreateDirWithDlcPermissions(module_path)) {
    LogAndSetError(err, "Failed to create DLC ID directory");
    return false;
  }
  // Create the DLC package directory with correct permissions.
  if (!CreateDirWithDlcPermissions(module_package_path)) {
    LogAndSetError(err, "Failed to create DLC ID package directory");
    return false;
  }

  // Creates DLC module storage.
  // TODO(xiaochu): Manifest currently returns a signed integer, which means
  // will likely fail for modules >= 2 GiB in size. https://crbug.com/904539
  imageloader::Manifest manifest;
  if (!dlcservice::utils::GetDlcManifest(manifest_dir_, id, package,
                                         &manifest)) {
    LogAndSetError(err, "Failed to get DLC module manifest.");
    return false;
  }
  int64_t image_size = manifest.preallocated_size();
  if (image_size <= 0) {
    LogAndSetError(err, "Preallocated size in manifest is illegal.");
    return false;
  }

  // Creates image A.
  FilePath image_a_path =
      utils::GetDlcModuleImagePath(content_dir_, id, package, 0);
  if (!CreateImageFile(image_a_path, image_size)) {
    LogAndSetError(err, "Failed to create slot A DLC image file");
    return false;
  }

  // Creates image B.
  FilePath image_b_path =
      utils::GetDlcModuleImagePath(content_dir_, id, package, 1);
  if (!CreateImageFile(image_b_path, image_size)) {
    LogAndSetError(err, "Failed to create slot B image file");
    return false;
  }

  *path = module_path;
  return true;
}

bool DlcServiceDBusAdaptor::DeleteDlc(brillo::ErrorPtr* err,
                                      const std::string& id) {
  FilePath dlc_module_path = utils::GetDlcModulePath(content_dir_, id);
  if (!DeleteFile(dlc_module_path, true)) {
    LogAndSetError(err, "DLC image folder could not be deleted.");
    return false;
  }
  return true;
}

bool DlcServiceDBusAdaptor::MountDlc(brillo::ErrorPtr* err,
                                     const string& id,
                                     string* mount_point) {
  if (!image_loader_proxy_->LoadDlcImage(id, ScanDlcModulePackage(id),
                                         current_boot_slot_name_, mount_point,
                                         nullptr)) {
    LogAndSetError(err, "Imageloader is not available.");
    return false;
  }
  if (mount_point->empty()) {
    LogAndSetError(err, "Imageloader LoadDlcImage() failed.");
    return false;
  }
  return true;
}

bool DlcServiceDBusAdaptor::UnmountDlc(brillo::ErrorPtr* err,
                                       const string& id) {
  bool success = false;
  if (!image_loader_proxy_->UnloadDlcImage(id, ScanDlcModulePackage(id),
                                           &success, nullptr)) {
    LogAndSetError(err, "Imageloader is not available.");
    return false;
  }
  if (!success) {
    LogAndSetError(err, "Imageloader UnloadDlcImage failed.");
    return false;
  }
  return true;
}

string DlcServiceDBusAdaptor::ScanDlcModulePackage(const string& id) {
  return *(utils::ScanDirectory(manifest_dir_.Append(id)).begin());
}

bool DlcServiceDBusAdaptor::CheckForUpdateEngineStatus(
    const vector<Operation>& status_list) {
  StatusResult status_result;
  if (!update_engine_proxy_->GetStatusAdvanced(&status_result, nullptr)) {
    LOG(ERROR) << "Update Engine is not available.";
    return false;
  }
  if (!std::any_of(status_list.begin(), status_list.end(),
                   [&status_result](const Operation& status) {
                     return status_result.current_operation() == status;
                   })) {
    return false;
  }
  return true;
}

void DlcServiceDBusAdaptor::SendOnInstalledSignal(
    const InstallResult& install_result) {
  org::chromium::DlcServiceInterfaceAdaptor::SendOnInstalledSignal(
      install_result);
}

void DlcServiceDBusAdaptor::OnStatusUpdateAdvancedSignal(
    const StatusResult& status_result) {
  if (!InstallingComplete(status_result))
    return;

  // At this point, update_engine finished installation of the requested DLC
  // modules (failure or success).
  DlcModuleList dlc_module_list = dlc_modules_being_installed_;
  dlc_modules_being_installed_.clear_dlc_module_infos();

  InstallResult install_result;
  install_result.set_success(false);
  install_result.mutable_dlc_module_list()->CopyFrom(dlc_module_list);

  // Keep track of the cleanups for DLC images.
  utils::ScopedCleanups<Callback<void()>> scoped_cleanups;
  for (const DlcModuleInfo& dlc_module : dlc_module_list.dlc_module_infos()) {
    const string& dlc_id = dlc_module.dlc_id();
    auto cleanup = Bind(
        [](Callback<bool()> unmounter, Callback<bool()> deleter) {
          unmounter.Run();
          deleter.Run();
        },
        base::Bind(&DlcServiceDBusAdaptor::UnmountDlc, base::Unretained(this),
                   nullptr, dlc_id),
        base::Bind(&DlcServiceDBusAdaptor::DeleteDlc, base::Unretained(this),
                   nullptr, dlc_id));
    scoped_cleanups.Insert(cleanup);
  }

  // Mount the installed DLC module images.
  for (auto& dlc_module :
       *install_result.mutable_dlc_module_list()->mutable_dlc_module_infos()) {
    const string& dlc_module_id = dlc_module.dlc_id();
    string mount_point;
    if (!MountDlc(nullptr, dlc_module_id, &mount_point)) {
      // This is to set |dlc_root|'s all back to empty strings.
      install_result.mutable_dlc_module_list()->CopyFrom(dlc_module_list);
      install_result.set_error_code(
          static_cast<int>(OnInstalledSignalErrorCode::kMountFailure));
      SendOnInstalledSignal(install_result);
      return;
    }
    dlc_module.set_dlc_root(
        utils::GetDlcRootInModulePath(FilePath(mount_point)).value());
  }

  // Don't unmount+delete the images+directories as all successfully installed.
  scoped_cleanups.Cancel();

  // Install was a success so keep track.
  for (const DlcModuleInfo& dlc_module : dlc_module_list.dlc_module_infos())
    installed_dlc_modules_.insert(dlc_module.dlc_id());

  install_result.set_success(true);
  install_result.set_error_code(
      static_cast<int>(OnInstalledSignalErrorCode::kNone));
  SendOnInstalledSignal(install_result);
}

void DlcServiceDBusAdaptor::OnStatusUpdateAdvancedSignalConnected(
    const string& interface_name, const string& signal_name, bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to connect to update_engine's StatusUpdate signal.";
  }
}

}  // namespace dlcservice
