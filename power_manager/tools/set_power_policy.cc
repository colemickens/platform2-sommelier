// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This program can be used to send a message to powerd to configure the
// power management policy.  This is the same mechanism used by Chrome; in
// fact, it will overwrite any policy set by Chrome.  To revert to powerd's
// default policy, run it without any arguments.

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/flag_helper.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

#include "power_manager/proto_bindings/policy.pb.h"

namespace {

const int kMsInSec = 1000;

// Given a command-line flag containing a duration in seconds, a
// power_manager::PowerManagementPolicy::Delays |submessage|, and the name
// of a milliseconds field in |submessage|, sets the field if the flag is
// greater than or equal to 0.
#define SET_DELAY_FIELD(flag, submessage, field) \
    if (flag >= 0) { \
      submessage->set_ ## field(flag * kMsInSec); \
    }

// Given a string from a flag describing an action, returns the
// corresponding value from power_manager::PowerManagementPolicy_Action.
power_manager::PowerManagementPolicy_Action GetAction(
    const std::string& action) {
  if (action == "suspend")
    return power_manager::PowerManagementPolicy_Action_SUSPEND;
  else if (action == "stop_session")
    return power_manager::PowerManagementPolicy_Action_STOP_SESSION;
  else if (action == "shut_down")
    return power_manager::PowerManagementPolicy_Action_SHUT_DOWN;
  else if (action == "do_nothing")
    return power_manager::PowerManagementPolicy_Action_DO_NOTHING;
  else
    LOG(FATAL) << "Invalid action \"" << action << "\"";
  return power_manager::PowerManagementPolicy_Action_DO_NOTHING;
}

}  // namespace

int main(int argc, char* argv[]) {
  // These mirror the fields from the PowerManagementPolicy protocol buffer.
  DEFINE_string(ac_idle_action, "",
                "Action to perform when idle on AC power (one of "
                "suspend, stop_session, shut_down, do_nothing)");
  DEFINE_string(battery_idle_action, "",
                "Action to perform when idle on battery power (one of "
                "suspend, stop_session, shut_down, do_nothing)");
  DEFINE_string(lid_closed_action, "",
                "Action to perform when lid is closed (one of "
                "suspend, stop_session, shut_down, do_nothing)");
  DEFINE_int32(ac_screen_dim_delay, -1,
               "Delay before dimming screen on AC power, in seconds");
  DEFINE_int32(ac_screen_off_delay, -1,
               "Delay before turning screen off on AC power, in seconds");
  DEFINE_int32(ac_screen_lock_delay, -1,
               "Delay before locking screen on AC power, in seconds");
  DEFINE_int32(ac_idle_warning_delay, -1,
               "Delay before idle action warning on AC power, in seconds");
  DEFINE_int32(ac_idle_delay, -1,
               "Delay before idle action on AC power, in seconds");
  DEFINE_int32(battery_screen_dim_delay, -1,
               "Delay before dimming screen on battery power, in seconds");
  DEFINE_int32(battery_screen_off_delay, -1,
               "Delay before turning screen off on battery power, in seconds");
  DEFINE_int32(battery_screen_lock_delay, -1,
               "Delay before locking screen on battery power, in seconds");
  DEFINE_int32(battery_idle_warning_delay, -1,
               "Delay before idle action warning on battery power, in seconds");
  DEFINE_int32(battery_idle_delay, -1,
               "Delay before idle action on battery power, in seconds");
  DEFINE_int32(use_audio_activity, -1,
               "Honor audio activity (1 is true, 0 is false, -1 is unset");
  DEFINE_int32(use_video_activity, -1,
               "Honor video activity (1 is true, 0 is false, -1 is unset");
  DEFINE_int32(wait_for_initial_user_activity, -1,
               "Wait for initial user activity before enforcing delays "
               "(1 is true, 0 is false, -1 is unset");
  DEFINE_double(ac_brightness_percent, -1.0,
                "Brightness percent to use while on AC power (less than 0.0 "
                "means unset)");
  DEFINE_double(battery_brightness_percent, -1.0,
                "Brightness percent to use while on battery power (less than "
                "0.0 means unset)");
  DEFINE_double(presentation_screen_dim_delay_factor, 0.0,
                "Factor by which the screen-dim delay is scaled while "
                "presenting (less than 1.0 means unset)");
  DEFINE_double(user_activity_screen_dim_delay_factor, 0.0,
                "Factor by which the screen-dim delay is scaled if user "
                "activity is observed while the screen is dimmed or soon after "
                "it's been turned off (less than 1.0 means unset)");

  chromeos::FlagHelper::Init(argc, argv,
      "Configures powerd's power management policy.\n\n"
      "When called without any arguments, uses default settings.");
  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;

  power_manager::PowerManagementPolicy policy;

  if (!FLAGS_ac_idle_action.empty())
    policy.set_ac_idle_action(GetAction(FLAGS_ac_idle_action));
  if (!FLAGS_battery_idle_action.empty())
    policy.set_battery_idle_action(GetAction(FLAGS_battery_idle_action));
  if (!FLAGS_lid_closed_action.empty())
    policy.set_lid_closed_action(GetAction(FLAGS_lid_closed_action));

  power_manager::PowerManagementPolicy::Delays* delays =
      policy.mutable_ac_delays();
  SET_DELAY_FIELD(FLAGS_ac_screen_dim_delay, delays, screen_dim_ms);
  SET_DELAY_FIELD(FLAGS_ac_screen_off_delay, delays, screen_off_ms);
  SET_DELAY_FIELD(FLAGS_ac_screen_lock_delay, delays, screen_lock_ms);
  SET_DELAY_FIELD(FLAGS_ac_idle_warning_delay, delays, idle_warning_ms);
  SET_DELAY_FIELD(FLAGS_ac_idle_delay, delays, idle_ms);

  delays = policy.mutable_battery_delays();
  SET_DELAY_FIELD(FLAGS_battery_screen_dim_delay, delays, screen_dim_ms);
  SET_DELAY_FIELD(FLAGS_battery_screen_off_delay, delays, screen_off_ms);
  SET_DELAY_FIELD(FLAGS_battery_screen_lock_delay, delays, screen_lock_ms);
  SET_DELAY_FIELD(FLAGS_battery_idle_warning_delay, delays, idle_warning_ms);
  SET_DELAY_FIELD(FLAGS_battery_idle_delay, delays, idle_ms);

  if (FLAGS_use_audio_activity >= 0)
    policy.set_use_audio_activity(FLAGS_use_audio_activity != 0);
  if (FLAGS_use_video_activity >= 0)
    policy.set_use_video_activity(FLAGS_use_video_activity != 0);
  if (FLAGS_ac_brightness_percent >= 0.0)
    policy.set_ac_brightness_percent(FLAGS_ac_brightness_percent);
  if (FLAGS_battery_brightness_percent >= 0.0)
    policy.set_battery_brightness_percent(FLAGS_battery_brightness_percent);
  if (FLAGS_presentation_screen_dim_delay_factor >= 1.0) {
    policy.set_presentation_screen_dim_delay_factor(
        FLAGS_presentation_screen_dim_delay_factor);
  }
  if (FLAGS_user_activity_screen_dim_delay_factor >= 1.0) {
    policy.set_user_activity_screen_dim_delay_factor(
        FLAGS_user_activity_screen_dim_delay_factor);
  }
  if (FLAGS_wait_for_initial_user_activity >= 0) {
    policy.set_wait_for_initial_user_activity(
        FLAGS_wait_for_initial_user_activity != 0);
  }

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));
  CHECK(bus->Connect());
  dbus::ObjectProxy* proxy = bus->GetObjectProxy(
      power_manager::kPowerManagerServiceName,
      dbus::ObjectPath(power_manager::kPowerManagerServicePath));

  dbus::MethodCall method_call(
      power_manager::kPowerManagerInterface,
      power_manager::kSetPolicyMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(policy);
  scoped_ptr<dbus::Response> response(
      proxy->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  CHECK(response.get());

  return 0;
}
