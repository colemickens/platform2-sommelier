// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>
#include <iostream>
#include <string>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_split.h>
#include <brillo/daemons/daemon.h>
#include <brillo/flag_helper.h>
#include <chromeos/constants/imageloader.h>
#include <dbus/bus.h>
#include <dlcservice/proto_bindings/dlcservice.pb.h>
#include <libimageloader/manifest.h>

#include "dlcservice/dbus-proxies.h"
#include "dlcservice/utils.h"

using org::chromium::DlcServiceInterfaceProxy;

class DlcServiceUtil : public brillo::Daemon {
 public:
  DlcServiceUtil(int argc, const char** argv)
      : argc_(argc), argv_(argv), current_index_(0), weak_ptr_factory_(this) {}
  ~DlcServiceUtil() {}

 private:
  int OnEventLoopStarted() override {
    DEFINE_bool(install, false, "Install a given list of DLC modules.");
    DEFINE_bool(uninstall, false, "Uninstall a given list of DLC modules.");
    DEFINE_bool(list, false, "List all installed DLC modules.");
    DEFINE_bool(oneline, false, "Print short module DLC module information.");
    DEFINE_string(dlc_ids, "", "Colon separated list of DLC module ids.");
    DEFINE_string(omaha_url, "",
                  "Overrides the default Omaha URL in the update_engine.");
    brillo::FlagHelper::Init(argc_, argv_, "dlcservice_util");

    // Enforce mutually exclusive flags. Additional exclusive flags can be added
    // to this list.
    std::vector<bool> exclusive_flags = {FLAGS_install, FLAGS_uninstall,
                                         FLAGS_list};
    if (std::count(exclusive_flags.begin(), exclusive_flags.end(), true) != 1) {
      LOG(ERROR)
          << "Exactly one of --install, --uninstall, --list must be set.";
      return EX_SOFTWARE;
    }

    int error = EX_OK;
    if (!Init(&error)) {
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

      // Set up callbacks
      omaha_url_ = FLAGS_omaha_url;
      dlc_ids_being_installed_ = dlc_id_list;
      current_index_ = 0;
      dlc_service_proxy_->RegisterOnInstalledSignalHandler(
          base::Bind(&DlcServiceUtil::OnInstalled,
                     weak_ptr_factory_.GetWeakPtr()),
          base::Bind(&DlcServiceUtil::OnInstalledConnect,
                     weak_ptr_factory_.GetWeakPtr()));

      if (Install()) {
        return EX_OK;
      }
      LOG(ERROR) << "Failed to install DLC modules.";
    } else if (FLAGS_uninstall) {
      std::vector<std::string> dlc_id_list = SplitString(
          FLAGS_dlc_ids, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (dlc_id_list.empty()) {
        LOG(ERROR) << "Please specify a list of DLC modules to uninstall.";
        return EX_SOFTWARE;
      }
      if (!Uninstall(dlc_id_list, &error)) {
        LOG(ERROR) << "Failed to uninstall DLC modules.";
        return error;
      }
    } else if (FLAGS_list) {
      dlcservice::DlcModuleList dlc_module_list;
      if (!GetInstalled(&dlc_module_list, &error)) {
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

    Quit();
    return EX_OK;
  }

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

  // Callback invoked on receiving |OnInstalled| signal.
  void OnInstalled(const std::string& install_result_str) {
    dlcservice::InstallResult install_result;
    if (!install_result.ParseFromString(install_result_str)) {
      LOG(ERROR) << "Error parsing protobuf OnInstalledSignal";
    } else if (!install_result.success()) {
      LOG(ERROR) << "Failed to install " << install_result.dlc_id()
                 << " with error code:" << install_result.error_code();
    } else {
      LOG(INFO) << "'" << install_result.dlc_id()
                << "' installed and available at '" << install_result.dlc_root()
                << "'.";
    }
    current_index_++;
    if (current_index_ >= dlc_ids_being_installed_.size()) {
      // Exit the process since all DLC modules are installed.
      Quit();
      return;
    }

    if (!Install())
      QuitWithExitCode(EX_SOFTWARE);
  }

  // Callback invoked on connecting |OnInstalled| signal.
  void OnInstalledConnect(const std::string& interface_name,
                          const std::string& signal_name,
                          bool success) {
    if (!success) {
      LOG(ERROR) << "Error connecting " << interface_name << "." << signal_name;
      QuitWithExitCode(EX_SOFTWARE);
    }
  }

  // Install current DLC module. Returns true if current module can be
  // installed. False otherwise.
  bool Install() {
    brillo::ErrorPtr error;

    LOG(INFO) << "Attempting to install DLC module '"
              << dlc_ids_being_installed_[current_index_] << "'.";
    if (!dlc_service_proxy_->Install(dlc_ids_being_installed_[current_index_],
                                     omaha_url_, &error)) {
      LOG(ERROR) << "Failed to install '"
                 << dlc_ids_being_installed_[current_index_] << "', "
                 << error->GetMessage();
      return false;
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
    base::FilePath manifest_root =
        base::FilePath(imageloader::kDlcManifestRootpath);
    // TODO(ahassani): This is a workaround. We need to get the list of packages
    // in the GetInstalled() or a separate signal. But for now since we just
    // have one package per DLC, this would work.
    std::vector<std::string> packages =
        dlcservice::utils::ScanDirectory(manifest_root);
    auto package = packages[0];

    imageloader::Manifest manifest;
    // TODO(ahassani): We have imported the entire dlcservice library just to
    // use this function. A better way to do this is to create a utility library
    // that is shared between dlcservice_util and dlcservice and use the
    // functions from there.
    if (!dlcservice::utils::GetDlcManifest(manifest_root, dlc_id, package,
                                           &manifest)) {
      LOG(ERROR) << "Failed to get DLC module manifest.";
      return false;
    }
    std::cout << "\tname: " << manifest.name() << std::endl;
    std::cout << "\tid: " << manifest.id() << std::endl;
    std::cout << "\tpackage: " << manifest.package() << std::endl;
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

  std::unique_ptr<DlcServiceInterfaceProxy> dlc_service_proxy_;

  // argc and argv passed to main().
  int argc_;
  const char** argv_;

  // A list of DLC module IDs being installed.
  std::vector<std::string> dlc_ids_being_installed_;
  // Index of the current DLC module being installed.
  int current_index_;
  // Customized Omaha server URL (empty being the default URL).
  std::string omaha_url_;

  base::WeakPtrFactory<DlcServiceUtil> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DlcServiceUtil);
};

int main(int argc, const char** argv) {
  DlcServiceUtil client(argc, argv);
  return client.Run();
}
