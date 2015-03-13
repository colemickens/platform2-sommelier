// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <string>

#include <base/macros.h>

#include "libprotobinder/binder_daemon.h"
#include "libprotobinder/binder_host.h"
#include "libprotobinder/parcel.h"
#include "libprotobinder/protobinder.h"

namespace protobinder {

class PingHost : public BinderHost {
 public:
  PingHost() : BinderHost() {}

 protected:
  ~PingHost() override {}

  // Called by BinderManager.
  int OnTransact(uint32_t code,
                 Parcel* data,
                 Parcel* reply,
                 uint32_t flags) override {
    LOG(INFO) << "code " << code;
    if (code == 1) {
      return SUCCESS;
    }
    return ERROR_UNKNOWN_CODE;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PingHost);
};

}  // namespace protobinder

int main() {
  protobinder::BinderDaemon daemon("ping");
  daemon.Init(new protobinder::PingHost());
  return daemon.Run();
}
