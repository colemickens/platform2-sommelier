// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <base/bind.h>
#include <base/command_line.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/syslog_logging.h>
#include <psyche/libpsyche/psyche_daemon.h>

#include "germ/germ_host.h"
#include "germ/process_reaper.h"

// TODO(usanghi): Find a better way to instantiate PsycheDaemon without
// extending it in each service.
namespace germ {

class GermDaemon : public psyche::PsycheDaemon {
 public:
  GermDaemon() {}
  ~GermDaemon() override {}

 private:
  // Implement PsycheDaemon
  int OnInit() override {
    process_reaper_.RegisterWithDaemon(this);

    int return_code = PsycheDaemon::OnInit();
    if (return_code != 0) {
      LOG(ERROR) << "Could not initialize daemon.";
      return return_code;
    }
    if (!psyche_connection()->RegisterService("germ", &host_)) {
      LOG(ERROR) << "Could not register with psyche.";
      return 1;
    }

    return 0;
  }

  GermHost host_;
  ProcessReaper process_reaper_;

  DISALLOW_COPY_AND_ASSIGN(GermDaemon);
};

}  // namespace germ

int main(int argc, char *argv[]) {
  base::CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog);
  germ::GermDaemon daemon;
  return daemon.Run();
}
