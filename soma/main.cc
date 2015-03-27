// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_path.h>
#include <base/memory/scoped_ptr.h>
#include <psyche/libpsyche/psyche_daemon.h>

#include "soma/libsoma/constants.h"
#include "soma/soma.h"

namespace {

const char kContainerSpecDir[] = "/etc/container_specs";

}  // namespace

// TODO(usanghi): Find a better way to instantiate PsycheDaemon without
// extending it in each service.
namespace soma {
class SomaDaemon : public psyche::PsycheDaemon {
 public:
  SomaDaemon() : soma_host_(base::FilePath(kContainerSpecDir)) {}
  ~SomaDaemon() override {}

 private:
  // Implement PsycheDaemon
  int OnInit() override {
    int return_code = PsycheDaemon::OnInit();
    if (return_code != 0) {
      LOG(ERROR) << "Error initializing Daemon.";
      return return_code;
    }
    if (!psyche_connection()->RegisterService(kSomaServiceName,
                                              &soma_host_)) {
      LOG(ERROR) << "Error registering self with psyche.";
      return -1;
    }
    return 0;
  }

  Soma soma_host_;

  DISALLOW_COPY_AND_ASSIGN(SomaDaemon);
};

}  // namespace soma

int main() {
  soma::SomaDaemon daemon;
  return daemon.Run();
}
