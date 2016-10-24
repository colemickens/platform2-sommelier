// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <brillo/daemons/dbus_daemon.h>
#include <brillo/dbus/async_event_sequencer.h>
#include <chromeos/dbus/service_constants.h>
#include <install_attributes/libinstallattributes.h>

#include "authpolicy/org.chromium.AuthPolicy.h"

using brillo::dbus_utils::AsyncEventSequencer;

namespace org {
namespace chromium {
namespace authpolicy {

const char kObjectServicePath[] = "/org/chromium/AuthPolicy/ObjectManager";

class Domain : public org::chromium::AuthPolicyInterface {
 public:
  explicit Domain(brillo::dbus_utils::ExportedObjectManager* object_manager)
      : dbus_object_{new brillo::dbus_utils::DBusObject{
        object_manager, object_manager->GetBus(),
            org::chromium::AuthPolicyAdaptor::GetObjectPath()}} {
      }

  ~Domain() override = default;
  bool AuthenticateUser(
      brillo::ErrorPtr* error,
      const std::string& in_user_principal_name,
      const dbus::FileDescriptor& in_password_fd,
      int32_t* out_code,
      std::string* account_id) override {
    return false;
  }

  bool JoinADDomain(
      brillo::ErrorPtr* error,
      const std::string& in_machine_name,
      const std::string& in_user_principal_name,
      const dbus::FileDescriptor& in_password_fd,
      int32_t* out_code) override {
    return false;
  }

  void RegisterAsync(
      const AsyncEventSequencer::CompletionAction& completion_callback) {
    scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
    dbus_adaptor_.RegisterWithDBusObject(dbus_object_.get());
    dbus_object_->RegisterAsync(
              sequencer->GetHandler("Failed exporting Domain.", true));
    sequencer->OnAllTasksCompletedCall({completion_callback});
  }

 private:
  org::chromium::AuthPolicyAdaptor dbus_adaptor_{this};
  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;
};

class Daemon : public brillo::DBusServiceDaemon {
 public:
  Daemon() : DBusServiceDaemon(::authpolicy::kAuthPolicyInterface,
    kObjectServicePath) {
  }

 protected:
  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    domain_.reset(new Domain(object_manager_.get()));
    domain_->RegisterAsync(
        sequencer->GetHandler("Domain.RegisterAsync() failed.", true));
  }

  void OnShutdown(int* return_code) override {
    DBusServiceDaemon::OnShutdown(return_code);
    domain_.reset();
  }

 private:
  std::unique_ptr<Domain> domain_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace authpolicy
}  // namespace chromium
}  // namespace org

int main() {
  // Safety check to ensure that authpolicyd cannot run after the device has
  // been locked to a mode other than enterprise_ad.  (The lifetime management
  // of authpolicyd happens through upstart, this check only serves as a second
  // line of defense.)
  InstallAttributesReader install_attributes_reader;
  if (install_attributes_reader.IsLocked()) {
    const std::string& mode =
        install_attributes_reader.GetAttribute(
            InstallAttributesReader::kAttrMode);
    if (mode != InstallAttributesReader::kDeviceModeEnterpriseAD) {
      LOG(ERROR) << "OOBE completed but device not in AD management mode.";
      // Exit with "success" to prevent respawn by upstart.
      exit(0);
    }
  }

  org::chromium::authpolicy::Daemon daemon;
  return daemon.Run();
}
