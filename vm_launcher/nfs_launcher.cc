// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_launcher/constants.h"
#include "vm_launcher/nfs_launcher.h"

#include <sys/signal.h>
#include <base/logging.h>
#include <base/files/file_util.h>
#include <brillo/process.h>
#include <brillo/syslog_logging.h>


namespace vm_launcher {

bool NfsLauncher::Terminate() {
  brillo::ProcessImpl nfs_upstart;
  nfs_upstart.AddArg("/sbin/stop");
  nfs_upstart.AddArg(vm_launcher::kGaneshaJobName);

  LOG(INFO) << "Stopping NFS server";
  if (!nfs_upstart.Run())
  {
    PLOG(ERROR) << "Unable to stop NFS server";
    return false;
  }
  return true;
}

bool NfsLauncher::Configure() {
  // For now, a single export sufficies. When having more VMs and maintaining
  // their state, the following config needs to be modified to export per VM.
  std::string config = R"XXX(
NFSV4 {
  Grace_Period = 1;
  #Graceless = true;
}
EXPORT
{
  Export_Id = 1366;
  Path = /home/chronos/user; #jail address
  Pseudo = /export;
  Squash = Root;
  Anonymous_Uid = 1000; #chronos
  Anonymous_Gid = 1000;
  Access_Type = RW;
  FSAL {
    Name = VFS;
  }
}
)XXX";

  base::FilePath config_directory(vm_launcher::kGaneshaConfigDirectory);
  if (!base::DirectoryExists(config_directory)) {
    LOG(INFO) << "Config directory " << vm_launcher::kGaneshaConfigDirectory
              << "does not exist, creating.";
    if (!base::CreateDirectory(config_directory)) {
      PLOG(ERROR) << "Unable to create config directory";
      return false;
    }
  }
  base::FilePath config_file_path =
      base::FilePath(vm_launcher::kGaneshaConfigDirectory)
      .Append(base::FilePath("ganesha.conf"));
  bool write_result = base::WriteFile(config_file_path, config.c_str(),
                                      config.length());
  if (!write_result)
    PLOG(ERROR) << "Unable to write config file " << config_file_path.value();
  return write_result;
}

bool NfsLauncher::Launch() {
  // Set up the configuration file before launching nfs server.
  if (!Configure())
    return false;

  brillo::ProcessImpl nfs_upstart;
  nfs_upstart.AddArg("/sbin/start");
  nfs_upstart.AddArg(vm_launcher::kGaneshaJobName);

  LOG(INFO) << "Starting NFS server";

  running_ = (nfs_upstart.Run() == 0);

  return running_;
}

NfsLauncher::~NfsLauncher() {
  if (running_)
    Terminate();
}

}  // namespace vm_launcher
