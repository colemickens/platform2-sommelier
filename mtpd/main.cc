// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple daemon to detect and access PTP/MTP devices.

#include <sysexits.h>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/string_number_conversions.h>
#include <brillo/daemons/dbus_daemon.h>
#include <brillo/syslog_logging.h>
#include <chromeos/dbus/service_constants.h>

#include "mtpd/mtpd_server_impl.h"

using base::CommandLine;
using base::MessageLoopForIO;

namespace {

// Messages logged at a level lower than this don't get logged anywhere.
static const char kMinLogLevelSwitch[] = "minloglevel";

void SetupLogging() {
  brillo::InitLog(brillo::kLogToSyslog);

  std::string log_level_str =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kMinLogLevelSwitch);
  int log_level = 0;
  if (base::StringToInt(log_level_str, &log_level) && log_level >= 0)
    logging::SetMinLogLevel(log_level);
}

}  // namespace

namespace mtpd {

class Daemon : public brillo::DBusServiceDaemon,
               public MessageLoopForIO::Watcher {
 public:
  Daemon() : DBusServiceDaemon(kMtpdServiceName) {}

 protected:
  // brillo::DBusServiceDaemon overrides.
  void RegisterDBusObjectsAsync(
      brillo::dbus_utils::AsyncEventSequencer* sequencer) override {
    adaptor_.reset(new MtpdServer(bus_));
    adaptor_->RegisterAsync(sequencer->GetHandler("RegisterAsync() failed",
                                                  true));
  }

  int OnInit() override {
    int exit_code = DBusServiceDaemon::OnInit();
    if (exit_code != EX_OK)
      return exit_code;

    MessageLoopForIO::current()->WatchFileDescriptor(
        adaptor_->GetDeviceEventDescriptor(),
        true, MessageLoopForIO::WATCH_READ, &watcher_, this);
    return EX_OK;
  }

  void OnShutdown(int* exit_code) override {
    watcher_.StopWatchingFileDescriptor();
    DBusServiceDaemon::OnShutdown(exit_code);
  }

  // MessageLoopForIO::Watcher overrides.
  void OnFileCanReadWithoutBlocking(int fd) override {
    adaptor_->ProcessDeviceEvents();
  }

  void OnFileCanWriteWithoutBlocking(int fd) override {}

 private:
  std::unique_ptr<MtpdServer> adaptor_;
  MessageLoopForIO::FileDescriptorWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace mtpd

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  SetupLogging();

  return mtpd::Daemon().Run();
}
