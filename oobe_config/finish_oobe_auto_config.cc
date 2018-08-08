// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/strings/string_util.h>
#include <brillo/flag_helper.h>
#include <brillo/process.h>

using base::FilePath;
using std::string;
using std::vector;

namespace {

constexpr char kOobeConfigDir[] = "unencrypted/oobe_auto_config/";
constexpr char kConfigFile[] = "config.json";
constexpr char kDomainFile[] = "enrollment_domain";
constexpr char kKeyFile[] = "validation_key.pub";

// TODO(tbrindus): move this into a util file.
int RunCommand(const vector<string>& command) {
  LOG(INFO) << "Command: " << base::JoinString(command, " ");

  brillo::ProcessImpl proc;

  for (const auto& arg : command) {
    proc.AddArg(arg);
  }

  return proc.Run();
}

// Using |priv_key|, signs |src|, and writes the digest into |dst|.
void SignFile(const FilePath& priv_key, const FilePath& src,
              const FilePath& dst) {
  CHECK(base::PathExists(src));
  LOG(INFO) << "Signing " << src.value() << " into " << dst.value();
  CHECK_EQ(RunCommand(vector<string> {
        "/usr/bin/openssl", "dgst",
        "-sha256",
        "-sign", priv_key.value(),
        "-out", dst.value(),
        src.value()
  }), 0);
}

void SignOobeFiles(const FilePath& priv_key, const FilePath& pub_key,
                   const FilePath& src_stateful, const FilePath& dst_stateful) {
  auto src_config_dir = src_stateful.Append(kOobeConfigDir);
  auto dst_config_dir = dst_stateful.Append(kOobeConfigDir);

  // /stateful/unencrypted/oobe_auto_config might not exist on the target
  // device, so create it here.
  CHECK(base::CreateDirectory(dst_config_dir));

  SignFile(priv_key, src_config_dir.Append(kConfigFile),
           dst_config_dir.Append(kConfigFile).AddExtension("sig"));

  // If the media was provisioned for auto-enrollment, sign the domain name
  // as well.
  if (base::PathExists(src_config_dir.Append(kDomainFile))) {
    SignFile(priv_key, src_config_dir.Append(kDomainFile),
             dst_config_dir.Append(kDomainFile).AddExtension("sig"));
  }
}

}  // namespace

int main(int argc, char** argv) {
  DEFINE_string(priv_key, "",
                "Path to private key to sign OOBE auto-configuration with.");
  DEFINE_string(pub_key, "",
                "Path to public key to validate OOBE auto-configuration with.");
  DEFINE_string(src_device, "", "Path to recovery media block device.");
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
      "* writes public key to target device stateful\n");

  FilePath priv_key(FLAGS_priv_key);
  FilePath pub_key(FLAGS_pub_key);
  FilePath src_device(FLAGS_src_device);
  FilePath src_stateful(FLAGS_src_stateful);
  FilePath dst_stateful(FLAGS_dst_stateful);

  CHECK(base::PathExists(priv_key));
  CHECK(base::PathExists(pub_key));
  CHECK(base::PathExists(src_device));
  CHECK(base::DirectoryExists(src_stateful));
  CHECK(base::DirectoryExists(dst_stateful));

  // Generate digests for the configuration and domain files.
  SignOobeFiles(priv_key, pub_key, src_stateful, dst_stateful);

  // Copy the public key into the target stateful for use in validation.
  CHECK(base::CopyFile(pub_key,
                       dst_stateful.Append(kOobeConfigDir).Append(kKeyFile)));
  return 0;
}
