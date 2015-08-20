// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <chromeos/flag_helper.h>

#include "settingsd/daemon.h"

namespace {

const char kDefaultSystemStoragePath[] = "/var/lib/settingsd/system";
const char kDefaultTrustedDocumentPath[] = "/etc/settingsd/system_config";

}  // namespace

int main(int argc, char* argv[]) {
  DEFINE_string(system_storage_path, "",
                "Path to directory where settings blobs for system-wide "
                "configuration are stored.");
  DEFINE_string(trusted_document_path, "",
                "Path to file containing the initial trusted document.");

  chromeos::FlagHelper::Init(argc, argv, "Settingsd daemon");
  if (FLAGS_system_storage_path.empty())
    FLAGS_system_storage_path = kDefaultSystemStoragePath;
  if (FLAGS_trusted_document_path.empty())
    FLAGS_trusted_document_path = kDefaultTrustedDocumentPath;

  settingsd::ConfigPaths config_paths;
  config_paths.system_storage = base::FilePath{FLAGS_system_storage_path};
  config_paths.trusted_document = base::FilePath{kDefaultTrustedDocumentPath};

  settingsd::Daemon daemon{config_paths};
  return daemon.Run();
}
