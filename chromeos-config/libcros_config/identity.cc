// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Look up identity information for the current device
// Also provide a way to fake identity for testing.

#include "chromeos-config/libcros_config/identity.h"

#include <string>

#include <base/logging.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include "chromeos-config/libcros_config/cros_config_interface.h"

namespace brillo {

CrosConfigIdentity::CrosConfigIdentity() {}

CrosConfigIdentity::~CrosConfigIdentity() {}

bool CrosConfigIdentity::FakeVpd(const std::string& vpd_id,
                                 base::FilePath* vpd_file_out) {
  *vpd_file_out = base::FilePath("vpd");
  if (base::WriteFile(*vpd_file_out, vpd_id.c_str(), vpd_id.length()) !=
      vpd_id.length()) {
    CROS_CONFIG_LOG(ERROR) << "Failed to write VPD file";
    return false;
  }

  return true;
}

bool CrosConfigIdentity::ReadVpd(const base::FilePath& vpd_file) {
  if (!base::ReadFileToString(vpd_file, &vpd_id_)) {
    CROS_CONFIG_LOG(WARNING) << "No identifier in VPD";
    // This file is only used for whitelabels, so may be missing. Without it
    // we rely on just the name and SKU ID.
  }
  CROS_CONFIG_LOG(INFO) << "Read VPD identity from  " << vpd_file.MaybeAsASCII()
                        << ": " << vpd_id_;
  return true;
}

}  // namespace brillo
