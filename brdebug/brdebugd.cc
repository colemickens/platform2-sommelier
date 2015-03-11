// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>
#include <memory>

#include <chromeos/daemons/dbus_daemon.h>

#include "brdebug/peerd_client.h"

namespace brdebug {

class Daemon : public chromeos::DBusDaemon {
 public:
  Daemon() {}
  ~Daemon() {}

 protected:
  int OnInit() override {
    int return_code = DBusDaemon::OnInit();
    if (return_code != EX_OK) return return_code;
    peerd_client_.reset(new PeerdClient(bus_));
    return return_code;
  }

 private:
  std::unique_ptr<PeerdClient> peerd_client_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace brdebug

int main() {
  brdebug::Daemon daemon;
  LOG(INFO) << "Starting daemon.";
  daemon.Run();
  LOG(INFO) << "Daemon stopped.";
  return 0;
}
