// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <memory>

#include <base/files/file_util.h>
#include <brillo/daemons/dbus_daemon.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "smbprovider/constants.h"
#include "smbprovider/samba_interface_impl.h"
#include "smbprovider/smbprovider.h"

namespace smbprovider {

namespace {

// Helper method to set $HOME variable to a temporary path that only
// smbproviderd user can access.
bool SetHomeEnvironmentVariable() {
  if (setenv(kHomeEnvironmentVariable, kSmbProviderHome, 1 /* overwrite */) !=
      0) {
    PLOG(ERROR) << "Failed to set $HOME variable";
    return false;
  }
  return true;
}

void InitLog() {
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);
  logging::SetLogItems(true /* enable_process_id */,
                       true /* enable_thread_id */, true /* enable_timestamp */,
                       true /* enable_tickcount */);
}

// Creates smb configuration file in $HOME/.smb/smb.conf.
bool CreateSmbConfFile() {
  base::File::Error ferror;
  const std::string smb_conf_directory(std::string(kSmbProviderHome) +
                                       kSmbConfLocation);
  if (!base::CreateDirectoryAndGetError(base::FilePath(smb_conf_directory),
                                        &ferror)) {
    LOG(ERROR) << "Failed to create directory '" << smb_conf_directory
               << "': " << base::File::ErrorToString(ferror);
    return false;
  }
  const int data_size = strlen(kSmbConfData);
  return base::WriteFile(base::FilePath(smb_conf_directory + kSmbConfFile),
                         kSmbConfData, data_size) == data_size;
}

}  // namespace

class SmbProviderDaemon : public brillo::DBusServiceDaemon {
 public:
  SmbProviderDaemon() : DBusServiceDaemon(kSmbProviderServiceName) {}

 protected:
  void RegisterDBusObjectsAsync(
      brillo::dbus_utils::AsyncEventSequencer* sequencer) override {
    std::unique_ptr<SambaInterfaceImpl> samba_interface =
        SambaInterfaceImpl::Create();
    if (!samba_interface) {
      LOG(ERROR) << "SambaInterface failed to initialize";
      exit(EXIT_FAILURE);
    }

    smb_provider_ = std::make_unique<SmbProvider>(
        std::make_unique<brillo::dbus_utils::DBusObject>(
            nullptr, bus_, org::chromium::SmbProviderAdaptor::GetObjectPath()),
        std::move(samba_interface));
    smb_provider_->RegisterAsync(
        sequencer->GetHandler("SmbProvider.RegisterAsync() failed.", true));
  }

  void OnShutdown(int* return_code) override {
    DBusServiceDaemon::OnShutdown(return_code);
    smb_provider_.reset();
  }

 private:
  std::unique_ptr<SmbProvider> smb_provider_;
  DISALLOW_COPY_AND_ASSIGN(SmbProviderDaemon);
};

// Runs SmbProviderDaemon.
int RunDaemon() {
  SmbProviderDaemon daemon;
  int res = daemon.Run();
  LOG(INFO) << "smbproviderd stopping with exit code " << res;
  return res;
}

}  // namespace smbprovider

int main(int argc, char* argv[]) {
  smbprovider::InitLog();
  // Smb configuration file must be written before the daemon is started because
  // the check for smb.conf happens when the context is set.
  if (!(smbprovider::SetHomeEnvironmentVariable() &&
        smbprovider::CreateSmbConfFile())) {
    LOG(ERROR) << "Failed to set configuration file, exiting";
    return EXIT_FAILURE;
  }
  return smbprovider::RunDaemon();
}
