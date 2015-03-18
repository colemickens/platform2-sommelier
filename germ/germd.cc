// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <base/macros.h>
#include <base/memory/scoped_ptr.h>

#include <protobinder/binder_daemon.h>
#include <protobinder/binder_host.h>
#include <protobinder/parcel.h>
#include <protobinder/protobinder.h>

#include "germ/launcher.h"

namespace germ {

class LaunchServiceHost : public protobinder::BinderHost {
 public:
  LaunchServiceHost() = default;

 protected:
  ~LaunchServiceHost() override {}

  // Called by BinderManager.
  int OnTransact(uint32_t code,
                 protobinder::Parcel* data,
                 protobinder::Parcel* reply,
                 uint32_t flags) override {
    // TODO(jorgelo): Use BIDL.
    return ERROR_UNKNOWN_CODE;
  }

 private:
  Launcher launcher_;

  DISALLOW_COPY_AND_ASSIGN(LaunchServiceHost);
};

}  // namespace germ

int main() {
  scoped_ptr<protobinder::IBinder> launch_service_host(
      new germ::LaunchServiceHost());
  protobinder::BinderDaemon daemon("germ", launch_service_host.Pass());
  return daemon.Run();
}
