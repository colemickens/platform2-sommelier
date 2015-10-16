// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <brillo/flag_helper.h>

#include "fides/daemon.h"

namespace {

const char kDefaultSystemStoragePath[] = "/var/lib/fidesd/system";
const char kDefaultTrustedDocumentPath[] = "/etc/fidesd/system_config";

}  // namespace

int main(int argc, char* argv[]) {
  DEFINE_string(system_storage_path, "",
                "Path to directory where settings blobs for system-wide "
                "configuration are stored.");
  DEFINE_string(trusted_document_path, "",
                "Path to file containing the initial trusted document.");

  brillo::FlagHelper::Init(argc, argv, "Fides daemon");
  if (FLAGS_system_storage_path.empty())
    FLAGS_system_storage_path = kDefaultSystemStoragePath;
  if (FLAGS_trusted_document_path.empty())
    FLAGS_trusted_document_path = kDefaultTrustedDocumentPath;

  fides::ConfigPaths config_paths;
  config_paths.system_storage = FLAGS_system_storage_path;
  config_paths.trusted_document = FLAGS_trusted_document_path;

  fides::Daemon daemon{config_paths};
  return daemon.Run();
}
