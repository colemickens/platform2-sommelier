// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>
#include <iostream>
#include <string>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_split.h>
#include <brillo/flag_helper.h>
#include <dbus/bus.h>
#include <libimageloader/manifest.h>

#include "dlcservice/dbus-proxies.h"
#include "dlcservice/proto_bindings/dlcservice.pb.h"
#include "dlcservice/utils.h"

using org::chromium::DlcServiceInterfaceProxy;

class DlcServiceUtil {
 public:
  DlcServiceUtil() {}

  ~DlcServiceUtil() {}

  // Initialize the dlcservice proxy. Returns true on success, false otherwise.
  // Sets the given error pointer on failure.
  bool Init(int* error_ptr) {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    scoped_refptr<dbus::Bus> bus{new dbus::Bus{options}};
    if (!bus->Connect()) {
      LOG(ERROR) << "Failed to connect to DBus.";
      *error_ptr = EX_UNAVAILABLE;
      return false;
    }
    dlc_service_proxy_ = std::make_unique<DlcServiceInterfaceProxy>(bus);

    return true;
  }

  // Install a list of DLC modules. Returns true if all modules install
  // successfully, false otherwise.
  bool Install(const std::vector<std::string>& dlc_ids, int* error_ptr) {
    brillo::ErrorPtr error;
    std::string mount_point;

    for (auto& dlc_id : dlc_ids) {
      LOG(INFO) << "Attempting to install DLC module '" << dlc_id << "'.";
      if (!dlc_service_proxy_->Install(dlc_id, &mount_point, &error)) {
        LOG(ERROR) << "Failed to install '" << dlc_id << "', "
                   << error->GetMessage();
        *error_ptr = EX_SOFTWARE;
        return false;
      }
      LOG(INFO) << "'" << dlc_id << "' successfully installed and mounted at '"
                << mount_point << "'.";
    }

    return true;
  }

  // Uninstall a list of DLC modules. Returns true of all uninstall operations
  // complete successfully, false otherwise. Sets the given error pointer on
  // failure.
  bool Uninstall(const std::vector<std::string>& dlc_ids, int* error_ptr) {
    brillo::ErrorPtr error;

    for (auto& dlc_id : dlc_ids) {
      LOG(INFO) << "Attempting to uninstall DLC module '" << dlc_id << "'.";
      if (!dlc_service_proxy_->Uninstall(dlc_id, &error)) {
        LOG(ERROR) << "Failed to uninstall '" << dlc_id << "', "
                   << error->GetMessage();
        *error_ptr = EX_SOFTWARE;
        return false;
      }
      LOG(INFO) << "'" << dlc_id << "' successfully uninstalled.";
    }

    return true;
  }

  // Retrieves a list of all installed DLC modules. Returns true if the list is
  // retrieved successfully, false otherwise. Sets the given error pointer on
  // failure.
  bool GetInstalled(dlcservice::DlcModuleList* dlc_module_list,
                    int* error_ptr) {
    std::string dlc_module_list_string;
    brillo::ErrorPtr error;

    if (!dlc_service_proxy_->GetInstalled(&dlc_module_list_string, &error)) {
      LOG(ERROR) << "Failed to get the list of installed DLC modules, "
                 << error->GetMessage();
      *error_ptr = EX_SOFTWARE;
      return false;
    }

    if (!dlc_module_list->ParseFromString(dlc_module_list_string)) {
      LOG(ERROR) << "Failed to parse DlcModuleList protobuf.";
      *error_ptr = EX_SOFTWARE;
      return false;
    }

    return true;
  }

  // Prints the information contained in the manifest of a DLC.
  static bool PrintDlcDetails(const std::string& dlc_id) {
    imageloader::Manifest manifest;
    if (!dlcservice::utils::GetDlcManifest(
            base::FilePath(dlcservice::kManifestDir), dlc_id, &manifest)) {
      LOG(ERROR) << "Failed to get DLC module manifest.";
      return false;
    }
    std::cout << "\tname: " << manifest.name() << std::endl;
    std::cout << "\tid: " << manifest.id() << std::endl;
    std::cout << "\tversion: " << manifest.version() << std::endl;
    std::cout << "\tmanifest version: " << manifest.manifest_version()
              << std::endl;
    std::cout << "\tpreallocated size: " << manifest.preallocated_size()
              << std::endl;
    std::cout << "\tsize: " << manifest.size() << std::endl;
    std::cout << "\timage type: " << manifest.image_type() << std::endl;
    std::cout << "\tremovable: " << (manifest.is_removable() ? "true" : "false")
              << std::endl;
    std::cout << "\tfs-type: ";
    switch (manifest.fs_type()) {
      case imageloader::FileSystem::kExt4:
        std::cout << "ext4" << std::endl;
        break;
      case imageloader::FileSystem::kSquashFS:
        std::cout << "squashfs" << std::endl;
        break;
    }
    return true;
  }

 private:
  std::unique_ptr<DlcServiceInterfaceProxy> dlc_service_proxy_;

  DISALLOW_COPY_AND_ASSIGN(DlcServiceUtil);
};

int main(int argc, const char** argv) {
  DEFINE_bool(install, false, "Install a given list of DLC modules.");
  DEFINE_bool(uninstall, false, "Uninstall a given list of DLC modules.");
  DEFINE_bool(list, false, "List all installed DLC modules.");
  DEFINE_bool(oneline, false, "Print short module DLC module information.");
  DEFINE_string(dlc_ids, "", "Colon separated list of DLC module ids.");
  brillo::FlagHelper::Init(argc, argv, "dlcservice_util");

  // Enforce mutually exclusive flags. Additional exclusive flags can be added
  // to this list.
  auto exclusive_flags = {FLAGS_install, FLAGS_uninstall, FLAGS_list};
  if (std::count(exclusive_flags.begin(), exclusive_flags.end(), true) != 1) {
    LOG(ERROR) << "Exactly one of --install, --uninstall, --list must be set.";
    return EX_SOFTWARE;
  }

  int error;
  DlcServiceUtil client;
  if (!client.Init(&error)) {
    LOG(ERROR) << "Failed to initialize client.";
    return error;
  }

  if (FLAGS_install) {
    std::vector<std::string> dlc_id_list = SplitString(
        FLAGS_dlc_ids, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (dlc_id_list.empty()) {
      LOG(ERROR) << "Please specify a list of DLC modules to install.";
      return EX_SOFTWARE;
    }
    if (!client.Install(dlc_id_list, &error)) {
      LOG(ERROR) << "Failed to install DLC modules.";
      return error;
    }
  } else if (FLAGS_uninstall) {
    std::vector<std::string> dlc_id_list = SplitString(
        FLAGS_dlc_ids, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (dlc_id_list.empty()) {
      LOG(ERROR) << "Please specify a list of DLC modules to uninstall.";
      return EX_SOFTWARE;
    }
    if (!client.Uninstall(dlc_id_list, &error)) {
      LOG(ERROR) << "Failed to uninstall DLC modules.";
      return error;
    }
  } else if (FLAGS_list) {
    dlcservice::DlcModuleList dlc_module_list;
    if (!client.GetInstalled(&dlc_module_list, &error)) {
      LOG(ERROR) << "Failed to get DLC module list.";
      return error;
    }
    std::cout << "Installed DLC modules:\n";
    for (const auto& dlc_module_info : dlc_module_list.dlc_module_infos()) {
      std::cout << dlc_module_info.dlc_id() << std::endl;
      if (!FLAGS_oneline) {
        if (!DlcServiceUtil::PrintDlcDetails(dlc_module_info.dlc_id()))
          LOG(ERROR) << "Failed to print details of DLC '"
                     << dlc_module_info.dlc_id() << "'.";
      }
    }
  }

  return EX_OK;
}
