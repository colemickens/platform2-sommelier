// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include <base/at_exit.h>
#include <base/bind.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/memory/scoped_refptr.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/time/time.h>
#include <brillo/flag_helper.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

#include "power_manager/proto_bindings/suspend.pb.h"

namespace {

// Human-readable description of the delay's purpose.
const char kSuspendDelayDescription[] = "suspend_delay_sample";

}  // namespace

// Passes |request| to powerd's |method_name| D-Bus method.
// Copies the returned protocol buffer to |reply_out|, which may be NULL if no
// reply is expected.
bool CallMethod(dbus::ObjectProxy* powerd_proxy,
                const std::string& method_name,
                const google::protobuf::MessageLite& request,
                google::protobuf::MessageLite* reply_out) {
  LOG(INFO) << "Calling " << method_name << " method";
  dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                               method_name);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(request);

  std::unique_ptr<dbus::Response> response(powerd_proxy->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response)
    return false;
  if (!reply_out)
    return true;

  dbus::MessageReader reader(response.get());
  CHECK(reader.PopArrayOfBytesAsProto(reply_out))
      << "Unable to parse response from call to " << method_name;
  return true;
}

// Registers a suspend delay and returns the corresponding ID.
int RegisterSuspendDelay(dbus::ObjectProxy* powerd_proxy, int timeout_ms) {
  power_manager::RegisterSuspendDelayRequest request;
  request.set_timeout(
      base::TimeDelta::FromMilliseconds(timeout_ms).ToInternalValue());
  request.set_description(kSuspendDelayDescription);
  power_manager::RegisterSuspendDelayReply reply;
  CHECK(CallMethod(powerd_proxy, power_manager::kRegisterSuspendDelayMethod,
                   request, &reply));
  LOG(INFO) << "Registered delay " << reply.delay_id();
  return reply.delay_id();
}

// Announces that the process is ready for suspend attempt |suspend_id|.
void SendSuspendReady(scoped_refptr<dbus::ObjectProxy> powerd_proxy,
                      int delay_id,
                      int suspend_id) {
  LOG(INFO) << "Announcing readiness of delay " << delay_id
            << " for suspend attempt " << suspend_id;
  power_manager::SuspendReadinessInfo request;
  request.set_delay_id(delay_id);
  request.set_suspend_id(suspend_id);
  CallMethod(powerd_proxy.get(), power_manager::kHandleSuspendReadinessMethod,
             request, nullptr);
}

// Handles the start of a suspend attempt. Posts a task to run
// SendSuspendReady() after a delay.
void HandleSuspendImminent(scoped_refptr<dbus::ObjectProxy> powerd_proxy,
                           int delay_id,
                           int delay_ms,
                           dbus::Signal* signal) {
  power_manager::SuspendImminent info;
  dbus::MessageReader reader(signal);
  CHECK(reader.PopArrayOfBytesAsProto(&info));
  int suspend_id = info.suspend_id();

  LOG(INFO) << "Got notification about suspend attempt " << suspend_id;
  LOG(INFO) << "Sleeping " << delay_ms << " ms before responding";
  base::MessageLoop::current()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SendSuspendReady, powerd_proxy, delay_id, suspend_id),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

// Handles the completion of a suspend attempt.
void HandleSuspendDone(dbus::Signal* signal) {
  power_manager::SuspendDone info;
  dbus::MessageReader reader(signal);
  CHECK(reader.PopArrayOfBytesAsProto(&info));
  const base::TimeDelta duration =
      base::TimeDelta::FromInternalValue(info.suspend_duration());
  LOG(INFO) << "Suspend attempt " << info.suspend_id() << " is complete; "
            << "system was suspended for " << duration.InMilliseconds()
            << " ms";
}

// Handles the result of an attempt to connect to a D-Bus signal.
void DBusSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool success) {
  CHECK(success) << "Unable to connect to " << interface << "." << signal;
}

int main(int argc, char* argv[]) {
  DEFINE_int32(delay_ms, 5000,
               "Milliseconds to wait before reporting suspend readiness");
  DEFINE_int32(timeout_ms, 7000, "Suspend timeout in milliseconds");

  brillo::FlagHelper::Init(
      argc, argv,
      "Exercise powerd's functionality that permits other processes to\n"
      "perform last-minute work before the system suspends.");
  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));
  CHECK(bus->Connect());

  dbus::ObjectProxy* powerd_proxy = bus->GetObjectProxy(
      power_manager::kPowerManagerServiceName,
      dbus::ObjectPath(power_manager::kPowerManagerServicePath));
  const int delay_id = RegisterSuspendDelay(powerd_proxy, FLAGS_timeout_ms);
  powerd_proxy->ConnectToSignal(
      power_manager::kPowerManagerInterface,
      power_manager::kSuspendImminentSignal,
      base::Bind(&HandleSuspendImminent, base::WrapRefCounted(powerd_proxy),
                 delay_id, FLAGS_delay_ms),
      base::Bind(&DBusSignalConnected));
  powerd_proxy->ConnectToSignal(
      power_manager::kPowerManagerInterface, power_manager::kSuspendDoneSignal,
      base::Bind(&HandleSuspendDone), base::Bind(&DBusSignalConnected));

  base::RunLoop().Run();

  // powerd will automatically unregister this process's suspend delay when the
  // process disconnects from D-Bus.
  return 0;
}
