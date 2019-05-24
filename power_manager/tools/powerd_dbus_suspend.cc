// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is meant for debugging use to manually trigger a proper
// suspend, excercising the full path through the power manager.
// The actual work to suspend the system is done by powerd_suspend.
// This tool will block and only exit after it has received a D-Bus
// resume signal from powerd.

#include <unistd.h>

#include <memory>

#include <base/at_exit.h>
#include <base/bind.h>
#include <base/command_line.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/strings/string_number_conversions.h>
#include <base/time/time.h>
#include <brillo/flag_helper.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

namespace {

// The sysfs entry that controls RTC wake alarms.  To set an alarm, write
// into this file the time of the alarm in seconds since the epoch.
const char kRtcWakeAlarmPath[] = "/sys/class/rtc/rtc0/wakealarm";

// Exits when powerd announces that the suspend attempt has completed.
void OnSuspendDone(dbus::Signal* signal) {
  base::MessageLoop::current()->QuitNow();
}

// Handles the result of an attempt to connect to a D-Bus signal.
void OnDBusSignalConnected(const std::string& interface,
                           const std::string& signal,
                           bool success) {
  CHECK(success) << "Unable to connect to " << interface << "." << signal;
}

// Invoked if a SuspendDone signal announcing resume isn't received before
// FLAGS_timeout.
void OnTimeout() {
  LOG(FATAL) << "Did not receive a " << power_manager::kSuspendDoneSignal
             << " signal within the timeout.";
}

}  // namespace

int main(int argc, char* argv[]) {
  DEFINE_int32(delay, 1,
               "Delay before suspending in seconds. Useful if "
               "running interactively to ensure that typing this command "
               "isn't recognized as user activity that cancels the suspend "
               "request.");
  DEFINE_int32(timeout, 0, "How long to wait for a resume signal in seconds.");
  DEFINE_uint64(wakeup_count, 0,
                "Wakeup count to pass to powerd or 0 if "
                "unset.");
  DEFINE_int32(wakeup_timeout, 0,
               "Sets an RTC alarm immediately that fires after the given "
               "interval. This ensures that device resumes while testing "
               "remotely.");
  DEFINE_int32(suspend_for_sec, 0,
               "Ask powerd to suspend the device for this many seconds. powerd "
               "then sets an alarm just before going to suspend.");

  brillo::FlagHelper::Init(argc, argv,
                           "Instruct powerd to suspend the system.");
  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));
  CHECK(bus->Connect());
  dbus::ObjectProxy* powerd_proxy = bus->GetObjectProxy(
      power_manager::kPowerManagerServiceName,
      dbus::ObjectPath(power_manager::kPowerManagerServicePath));

  if (FLAGS_delay)
    sleep(FLAGS_delay);

  // Set an RTC alarm to wake up the system.
  if (FLAGS_wakeup_timeout > 0) {
    std::string alarm_string = "+" + base::IntToString(FLAGS_wakeup_timeout);
    CHECK(base::WriteFile(base::FilePath(kRtcWakeAlarmPath),
                          alarm_string.c_str(), alarm_string.length()));
  }

  powerd_proxy->ConnectToSignal(
      power_manager::kPowerManagerInterface, power_manager::kSuspendDoneSignal,
      base::Bind(&OnSuspendDone), base::Bind(&OnDBusSignalConnected));

  // Send a suspend request.
  dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                               power_manager::kRequestSuspendMethod);
  if (FLAGS_wakeup_count) {
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(FLAGS_wakeup_count);
  }
  // Pass suspend_for_sec to the daemon.
  if (FLAGS_suspend_for_sec) {
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(FLAGS_suspend_for_sec);
  }
  std::unique_ptr<dbus::Response> response(powerd_proxy->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  CHECK(response) << power_manager::kRequestSuspendMethod << " failed";

  // Schedule a task to fire after the timeout.
  if (FLAGS_timeout) {
    base::MessageLoop::current()->task_runner()->PostDelayedTask(
        FROM_HERE, base::Bind(&OnTimeout),
        base::TimeDelta::FromSeconds(FLAGS_timeout));
  }

  base::RunLoop().Run();
  return 0;
}
