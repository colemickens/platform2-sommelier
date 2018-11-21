// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>
#include <string>
#include <vector>

#include <base/logging.h>
#include <brillo/flag_helper.h>

class DlcServiceUtil {
 public:
  DlcServiceUtil(const std::string& install_dlc_ids,
                 const std::string& uninstall_dlc_ids,
                 bool list_dlcs)
      : install_dlc_ids_(install_dlc_ids),
        uninstall_dlc_ids_(uninstall_dlc_ids),
        list_dlcs_(list_dlcs) {}

  ~DlcServiceUtil() {}

  bool Run(int* error_ptr) {
    if (!install_dlc_ids_.empty()) {
      return Install(error_ptr);
    } else if (!uninstall_dlc_ids_.empty()) {
      return Uninstall(error_ptr);
    } else if (list_dlcs_) {
      return GetInstalled(error_ptr);
    }

    return true;
  }

  bool Install(int* error_ptr) {
    std::vector<std::string> dlc_ids;
    if (!ParseDlcIds(install_dlc_ids_, &dlc_ids)) {
      LOG(ERROR) << "Please specify a list of DLC modules to install";
      *error_ptr = EX_USAGE;
      return false;
    }
    for (auto& s : dlc_ids) {
      LOG(INFO) << "Attempting to install DLC module '" << s << "'";
    }
    return true;
  }

  bool Uninstall(int* error_ptr) {
    std::vector<std::string> dlc_ids;
    if (!ParseDlcIds(uninstall_dlc_ids_, &dlc_ids)) {
      LOG(ERROR) << "Please specify a list of DLC modules to uninstall";
      *error_ptr = EX_USAGE;
      return false;
    }
    for (auto& s : dlc_ids) {
      LOG(INFO) << "Attempting to uninstall DLC module '" << s << "'";
    }
    return true;
  }

  bool GetInstalled(int* error_ptr) { return true; }

  bool ParseDlcIds(const std::string& dlc_ids,
                   std::vector<std::string>* dlc_ids_ptr) {
    std::vector<std::string>& dlc_ids_out = *dlc_ids_ptr;

    size_t start = 0;
    size_t end = 0;
    while (end < dlc_ids.length()) {
      end = dlc_ids.find(':', start);
      end = (end == std::string::npos) ? dlc_ids.length() : end;
      if (start < end)
        dlc_ids_out.emplace_back(dlc_ids.substr(start, end - start));
      start = end + 1;
    }

    return dlc_ids_out.size() > 0;
  }

  const std::string install_dlc_ids_;
  const std::string uninstall_dlc_ids_;
  const bool list_dlcs_;

  DISALLOW_COPY_AND_ASSIGN(DlcServiceUtil);
};

int main(int argc, const char** argv) {
  DEFINE_string(install, "", "Install a colon separated list of DLC modules");
  DEFINE_string(uninstall, "",
                "Uninstall a colon separated list of DLC modules");
  DEFINE_bool(list, false, "List all installed DLC modules");
  brillo::FlagHelper::Init(argc, argv, "dlcservice_util");

  if ((!FLAGS_install.empty() && !FLAGS_uninstall.empty()) ||
      (FLAGS_list && (!FLAGS_install.empty() || !FLAGS_uninstall.empty()))) {
    LOG(ERROR) << "Only one of --install, --uninstall, --list can be specified";
    return EX_USAGE;
  }

  DlcServiceUtil client{FLAGS_install, FLAGS_uninstall, FLAGS_list};

  int error_code;
  return client.Run(&error_code) ? EX_OK : error_code;
}
