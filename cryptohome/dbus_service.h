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
    adaptor_.reset(new UserDataAuthAdaptor(bus_, service_.get()));
    adaptor_->RegisterAsync(
        sequencer->GetHandler("RegisterAsync() failed", true));
  }

 private:
  std::unique_ptr<UserDataAuthAdaptor> adaptor_;

  std::unique_ptr<UserDataAuth> service_;

  DISALLOW_COPY_AND_ASSIGN(UserDataAuthDaemon);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_DBUS_SERVICE_H_
