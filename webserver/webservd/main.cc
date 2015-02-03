// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <sysexits.h>

#include <base/command_line.h>
#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/flag_helper.h>
#include <chromeos/minijail/minijail.h>
#include <chromeos/syslog_logging.h>

#include "webserver/webservd/server.h"

using chromeos::dbus_utils::AsyncEventSequencer;

namespace {

const char kServiceName[] = "org.chromium.WebServer";
const char kRootServicePath[] = "/org/chromium/WebServer";
const char kWebServerUserName[] = "webservd";
const char kWebServerGroupName[] = "webservd";

class Daemon : public chromeos::DBusServiceDaemon {
 public:
  Daemon(uint16_t http_port, uint16_t https_port, bool debug)
      : DBusServiceDaemon{kServiceName, kRootServicePath},
        http_port_{http_port},
        https_port_{https_port},
        debug_{debug} {}

 protected:
  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override {
    server_.reset(new webservd::Server{object_manager_.get(),
                                       http_port_,
                                       https_port_,
                                       debug_});
    server_->RegisterAsync(
        sequencer->GetHandler("Server.RegisterAsync() failed.", true));
  }

 private:
  uint16_t http_port_{0};
  uint16_t https_port_{0};
  bool debug_{false};
  std::unique_ptr<webservd::Server> server_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace

int main(int argc, char* argv[]) {
  DEFINE_int32(http_port, 80,
               "HTTP port to listen for requests on (0 to disable)");
  DEFINE_int32(https_port, 443,
               "HTTPS port to listen for requests on (0 to disable)");
  DEFINE_bool(log_to_stderr, false, "log trace messages to stderr as well");
  DEFINE_bool(debug, false,
              "return debug error information in web requests");
  chromeos::FlagHelper::Init(argc, argv, "Brillo web server daemon");

  int flags = chromeos::kLogToSyslog;
  if (FLAGS_log_to_stderr)
    flags |= chromeos::kLogToStderr;
  chromeos::InitLog(flags | chromeos::kLogHeader);

  if (FLAGS_http_port < 0 || FLAGS_http_port > 0xFFFF) {
    LOG(ERROR) << "Invalid HTTP port specified: '" << FLAGS_http_port << "'.";
    return EX_USAGE;
  }

  if (FLAGS_https_port < 0 || FLAGS_https_port > 0xFFFF) {
    LOG(ERROR) << "Invalid HTTPS port specified: '" << FLAGS_https_port << "'.";
    return EX_USAGE;
  }

  Daemon daemon{static_cast<uint16_t>(FLAGS_http_port),
                static_cast<uint16_t>(FLAGS_https_port),
                FLAGS_debug};

  // Drop privileges and use 'webservd' user. We need to do this after Daemon
  // object is constructed since it creates an instance of base::AtExitManager
  // which is required for chromeos::Minijail::GetInstance() to work.
  chromeos::Minijail* minijail_instance = chromeos::Minijail::GetInstance();
  minijail* jail = minijail_instance->New();
  minijail_instance->DropRoot(jail, kWebServerUserName, kWebServerGroupName);
  // Permissions needed for the daemon to allow it to bind to ports like TCP
  // 80.
  minijail_instance->UseCapabilities(jail, CAP_TO_MASK(CAP_NET_BIND_SERVICE));
  minijail_enter(jail);
  minijail_instance->Destroy(jail);
  return daemon.Run();
}
