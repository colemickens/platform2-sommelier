// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>
#include <string>
#include <syslog.h>

#include "shill/shill_logging.h"
#include "shill/shill_daemon.h"
#include "shill/dbus_control.h"

using std::string;

  /*
DEFINE_string(config_dir, "",
              "Directory to read confguration settings.");
DEFINE_string(default_config_dir, "",
              "Directory to read default configuration settings (Read Only).");
  */
namespace google {
class LogSinkSyslog : public google::LogSink {
 public:
  LogSinkSyslog() {
    openlog("shill",
            LOG_PID,  // Options
            LOG_LOCAL3);  // 5,6,7 are taken
  }

  virtual void send(LogSeverity severity, const char* /* full_filename */,
                    const char* base_filename, int line,
                    const struct ::tm* /* tm_time */,
                    const char* message, size_t message_len) {
    static const int glog_to_syslog[NUM_SEVERITIES] = {
      LOG_INFO, LOG_WARNING, LOG_ERR, LOG_CRIT};
    CHECK(severity < NUM_SEVERITIES && severity >= 0);

    syslog(glog_to_syslog[severity],
           "%s:%d %.*s",
           base_filename, line, message_len, message);
  }

  virtual ~LogSinkSyslog() {
    closelog();
  }
};
}  // namespace google


int main(int /* argc */, char** argv) {
  /*
  FilePath config_dir(FLAGS_config_dir);
  FilePath default_config_dir(FLAGS_default_config_dir.empty() ?
                              shill::Config::kShillDefaultPrefsDir :
                              FLAGS_default_config_dir);
  */
  google::LogSinkSyslog syslog_sink;

  google::InitGoogleLogging(argv[0]);
  google::AddLogSink(&syslog_sink);
  shill::Config config; /* (config_dir, default_config_dir) */

  // TODO(pstew): This should be chosen based on config
  shill::ControlInterface *control_interface = new shill::DBusControl();

  shill::Daemon daemon(&config, control_interface);
  daemon.Run();

  return 0;
}
