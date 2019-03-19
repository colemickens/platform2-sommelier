// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/anomaly_detector.h"

#include <pwd.h>
#include <unistd.h>

#include <string>

#include <base/at_exit.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop/message_loop.h>
#include <brillo/flag_helper.h>
#include <brillo/process.h>
#include <brillo/syslog_logging.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>

#include "metrics_event/proto_bindings/metrics_event.pb.h"

namespace {

// D-Bus handle for sending OOM-kill signals.  This is owned by |dbus| in
// main().
dbus::ExportedObject* g_dbus_exported_object = nullptr;

// Path to crash_reporter.
const char* crash_reporter_path = "/sbin/crash_reporter";

// Prepares for sending D-Bus signals.  Returns a D-Bus object, which provides
// a handle for sending signals.
scoped_refptr<dbus::Bus> SetUpDBus(void) {
  // Connect the bus.
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> dbus(new dbus::Bus(options));
  CHECK(dbus);
  CHECK(dbus->Connect()) << "Failed to connect to D-Bus";
  // Export a bus object so that other processes can register signal handlers
  // (this service only sends signals, no methods are exported).
  g_dbus_exported_object = dbus->GetExportedObject(
      dbus::ObjectPath(anomaly_detector::kAnomalyEventServicePath));
  CHECK(g_dbus_exported_object);
  return dbus;
}

}  // namespace

// Callback to run crash-reporter.
void RunCrashReporter(int filter, const char* flag, const char* input_path) {
  brillo::ProcessImpl cmd;
  cmd.RedirectInput(input_path);
  cmd.AddArg(crash_reporter_path);
  if (!filter)
    cmd.AddArg(flag);
  CHECK_EQ(0, cmd.Run());
}

void CDBusSendOomSignal(const int64_t oom_timestamp_ms) {
  dbus::Signal signal(anomaly_detector::kAnomalyEventServiceInterface,
                      anomaly_detector::kAnomalyEventSignalName);
  dbus::MessageWriter writer(&signal);
  metrics_event::Event payload;
  payload.set_type(metrics_event::Event_Type_OOM_KILL_KERNEL);
  payload.set_timestamp(oom_timestamp_ms);
  writer.AppendProtoAsArrayOfBytes(payload);
  CHECK(g_dbus_exported_object);
  g_dbus_exported_object->SendSignal(&signal);
}

int main(int argc, char* argv[]) {
  // Sim sala bim!  These are needed to send D-Bus signals.  Even though they
  // are not used directly, they set up some global state needed by the D-Bus
  // library.
  base::MessageLoop message_loop;
  base::AtExitManager at_exit_manager;

  DEFINE_bool(filter, false, "Input is stdin and output is stdout");
  DEFINE_bool(test, false, "Run self-tests");

  brillo::FlagHelper::Init(argc, argv, "Crash Helper: Anomaly Detector");
  brillo::OpenLog("anomaly_detector", true);
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  if (FLAGS_filter)
    crash_reporter_path = "/bin/cat";
  else if (FLAGS_test)
    crash_reporter_path = "./anomaly_detector_test_reporter.sh";

  scoped_refptr<dbus::Bus> dbus;
  if (!FLAGS_test)
    dbus = SetUpDBus();

  return AnomalyLexer(FLAGS_filter, FLAGS_test);
}
