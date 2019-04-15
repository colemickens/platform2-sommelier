// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_PROXY_DBUS_SERVICE_H_
#define CRYPTOHOME_PROXY_DBUS_SERVICE_H_

#include <memory>
#include <sysexits.h>

#include <brillo/daemons/dbus_daemon.h>

namespace cryptohome {

class CryptohomeProxyDaemon : public brillo::DBusDaemon {
 public:
  CryptohomeProxyDaemon() : DBusDaemon() {}

 protected:
  int OnInit() override {
    brillo::DBusDaemon::OnInit();
    return EX_OK;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptohomeProxyDaemon);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_PROXY_DBUS_SERVICE_H_
