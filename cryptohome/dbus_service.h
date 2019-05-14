// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_DBUS_SERVICE_H_
#define CRYPTOHOME_DBUS_SERVICE_H_

#include <memory>

#include <brillo/daemons/dbus_daemon.h>

#include "cryptohome/service_userdataauth.h"
#include "cryptohome/userdataauth.h"

namespace cryptohome {

class UserDataAuthDaemon : public brillo::DBusServiceDaemon {
 public:
  UserDataAuthDaemon()
      : DBusServiceDaemon(kUserDataAuthServiceName),
        service_(new cryptohome::UserDataAuth()) {
    // Initialize the User Data Auth service
    service_->Initialize();
  }

 protected:
  void RegisterDBusObjectsAsync(
      brillo::dbus_utils::AsyncEventSequencer* sequencer) override {
    DCHECK(!dbus_object_);
    dbus_object_ = std::make_unique<brillo::dbus_utils::DBusObject>(
        nullptr, bus_, dbus::ObjectPath(kUserDataAuthServicePath));

    userdataauth_adaptor_.reset(
        new UserDataAuthAdaptor(bus_, dbus_object_.get(), service_.get()));
    userdataauth_adaptor_->RegisterAsync();

    pkcs11_adaptor_.reset(
        new Pkcs11Adaptor(bus_, dbus_object_.get(), service_.get()));
    pkcs11_adaptor_->RegisterAsync();

    install_attributes_adaptor_.reset(
        new InstallAttributesAdaptor(bus_, dbus_object_.get(), service_.get()));
    install_attributes_adaptor_->RegisterAsync();

    misc_adaptor_.reset(
        new CryptohomeMiscAdaptor(bus_, dbus_object_.get(), service_.get()));
    misc_adaptor_->RegisterAsync();

    dbus_object_->RegisterAsync(
        sequencer->GetHandler("RegisterAsync() for UserDataAuth failed", true));
  }

 private:
  std::unique_ptr<UserDataAuthAdaptor> userdataauth_adaptor_;
  std::unique_ptr<Pkcs11Adaptor> pkcs11_adaptor_;
  std::unique_ptr<InstallAttributesAdaptor> install_attributes_adaptor_;
  std::unique_ptr<CryptohomeMiscAdaptor> misc_adaptor_;

  std::unique_ptr<UserDataAuth> service_;

  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;

  DISALLOW_COPY_AND_ASSIGN(UserDataAuthDaemon);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_DBUS_SERVICE_H_
