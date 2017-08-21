// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/daemons/dbus_daemon.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include <base/memory/ptr_util.h>
#include "smbprovider/samba_interface_impl.h"
#include "smbprovider/smbprovider.h"

namespace smbprovider {

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
    smb_provider_.reset(new SmbProvider(
        base::MakeUnique<brillo::dbus_utils::DBusObject>(
            nullptr, bus_, org::chromium::SmbProviderAdaptor::GetObjectPath()),
        std::move(samba_interface)));
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

}  // namespace smbprovider

int main(int argc, char* argv[]) {
  smbprovider::SmbProviderDaemon daemon;
  int res = daemon.Run();
  LOG(INFO) << "smbproviderd stopping with exit code " << res;

  return res;
}
