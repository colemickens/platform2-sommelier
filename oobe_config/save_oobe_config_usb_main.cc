// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_path.h>
#include <brillo/flag_helper.h>

#include "oobe_config/save_oobe_config_usb.h"
#include "oobe_config/usb_utils.h"

using base::FilePath;

int main(int argc, char** argv) {
  DEFINE_string(private_key, "",
                "Path to private key to sign OOBE auto-configuration with.");
  DEFINE_string(public_key, "",
                "Path to public key to validate OOBE auto-configuration with.");
  DEFINE_string(src_stateful_dev, "",
                "Path to the block device of recovery media's stateful"
                " partition.");
  DEFINE_string(src_stateful, "", "Path to recovery media stateful mount.");
  DEFINE_string(dst_stateful, "", "Path to target device stateful mount.");

  brillo::FlagHelper::Init(
      argc, argv,
      "finish_oobe_auto_config\n"
      "\n"
      "This utility performs OOBE auto-configuration setup in\n"
      "chromeos-installer.\n"
      "\n"
      "It:\n"
      "* creates $dst_stateful/unencrypted/oobe_auto_config/\n"
      "* signs $src_stateful/config.json\n"
      "* if it exists, signs $src_stateful/enrollment_domain\n"
      "* writes public key to target device stateful\n"
      "* determines a persistent block device for $src_stateful_dev and\n"
      "* writes a digest of it to target device stateful\n");

  CHECK_NE(FLAGS_private_key, "");
  CHECK_NE(FLAGS_public_key, "");
  CHECK_NE(FLAGS_src_stateful_dev, "");
  CHECK_NE(FLAGS_src_stateful, "");
  CHECK_NE(FLAGS_dst_stateful, "");

  oobe_config::SaveOobeConfigUsb config_saver(
      (FilePath(FLAGS_dst_stateful)), (FilePath(FLAGS_src_stateful)),
      (FilePath(oobe_config::kDevDiskById)), (FilePath(FLAGS_src_stateful_dev)),
      (FilePath(FLAGS_private_key)), (FilePath(FLAGS_public_key)));
  if (!config_saver.Save()) {
    return 1;
  }
  return 0;
}
