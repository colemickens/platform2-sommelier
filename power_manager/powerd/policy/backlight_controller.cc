// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include <base/bind.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>

#include "power_manager/powerd/policy/backlight_controller.h"

#include "power_manager/powerd/system/dbus_wrapper.h"

namespace power_manager {
namespace policy {

namespace {

void OnIncreaseBrightness(
    const BacklightController::IncreaseBrightnessCallback& callback,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  callback.Run();
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

void OnDecreaseBrightness(
    const BacklightController::DecreaseBrightnessCallback& callback,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  bool allow_off = true;
  if (!dbus::MessageReader(method_call).PopBool(&allow_off))
    allow_off = true;

  callback.Run(allow_off);
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

void OnSetBrightness(const std::string& method_name,
                     const BacklightController::SetBrightnessCallback& callback,
                     dbus::MethodCall* method_call,
                     dbus::ExportedObject::ResponseSender response_sender) {
  double percent = 0.0;
  using Transition = policy::BacklightController::Transition;
  Transition transition = Transition::FAST;
  SetBacklightBrightnessRequest_Cause cause =
      SetBacklightBrightnessRequest_Cause_USER_REQUEST;

  dbus::MessageReader reader(method_call);
  SetBacklightBrightnessRequest request;
  if (reader.PopArrayOfBytesAsProto(&request)) {
    percent = request.percent();
    switch (request.transition()) {
      case SetBacklightBrightnessRequest_Transition_INSTANT:
        transition = Transition::INSTANT;
        break;
      case SetBacklightBrightnessRequest_Transition_FAST:
        transition = Transition::FAST;
        break;
      case SetBacklightBrightnessRequest_Transition_SLOW:
        transition = Transition::SLOW;
        break;
    }
    cause = request.cause();
  } else {
    // TODO(derat): Delete this path and just return the error after Chrome has
    // been updated to send protobufs instead of D-Bus args:
    // https://crbug.com/881786
    int dbus_transition = 0;
    if (!reader.PopDouble(&percent) || !reader.PopInt32(&dbus_transition)) {
      LOG(ERROR) << "Invalid " << method_name << " args";
      response_sender.Run(dbus::ErrorResponse::FromMethodCall(
          method_call, DBUS_ERROR_INVALID_ARGS,
          "Expected SetBacklightBrightnessRequest protobuf"));
      return;
    }
    switch (dbus_transition) {
      case kBrightnessTransitionGradual:
        transition = Transition::FAST;
        break;
      case kBrightnessTransitionInstant:
        transition = Transition::INSTANT;
        break;
      default:
        LOG(ERROR) << "Invalid " << method_name << " transition "
                   << dbus_transition;
    }
  }

  callback.Run(percent, transition, cause);
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

void OnGetBrightness(const std::string& method_name,
                     const BacklightController::GetBrightnessCallback& callback,
                     dbus::MethodCall* method_call,
                     dbus::ExportedObject::ResponseSender response_sender) {
  double percent = 0.0;
  bool success = false;
  callback.Run(&percent, &success);
  if (!success) {
    response_sender.Run(dbus::ErrorResponse::FromMethodCall(
        method_call, DBUS_ERROR_FAILED, "Couldn't fetch brightness"));
    return;
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter(response.get()).AppendDouble(percent);
  response_sender.Run(std::move(response));
}

}  // namespace

// static
void BacklightController::RegisterIncreaseBrightnessHandler(
    system::DBusWrapperInterface* dbus_wrapper,
    const std::string& method_name,
    const IncreaseBrightnessCallback& callback) {
  DCHECK(dbus_wrapper);
  dbus_wrapper->ExportMethod(method_name,
                             base::Bind(&OnIncreaseBrightness, callback));
}

// static
void BacklightController::RegisterDecreaseBrightnessHandler(
    system::DBusWrapperInterface* dbus_wrapper,
    const std::string& method_name,
    const DecreaseBrightnessCallback& callback) {
  DCHECK(dbus_wrapper);
  dbus_wrapper->ExportMethod(method_name,
                             base::Bind(&OnDecreaseBrightness, callback));
}

// static
void BacklightController::RegisterSetBrightnessHandler(
    system::DBusWrapperInterface* dbus_wrapper,
    const std::string& method_name,
    const SetBrightnessCallback& callback) {
  DCHECK(dbus_wrapper);
  dbus_wrapper->ExportMethod(
      method_name, base::Bind(&OnSetBrightness, method_name, callback));
}

// static
void BacklightController::RegisterGetBrightnessHandler(
    system::DBusWrapperInterface* dbus_wrapper,
    const std::string& method_name,
    const GetBrightnessCallback& callback) {
  DCHECK(dbus_wrapper);
  dbus_wrapper->ExportMethod(
      method_name, base::Bind(&OnGetBrightness, method_name, callback));
}

// static
void BacklightController::EmitBrightnessChangedSignal(
    system::DBusWrapperInterface* dbus_wrapper,
    const std::string& signal_name,
    double brightness_percent,
    BacklightBrightnessChange_Cause cause) {
  DCHECK(dbus_wrapper);
  dbus::Signal signal(kPowerManagerInterface, signal_name);
  BacklightBrightnessChange proto;
  proto.set_percent(brightness_percent);
  proto.set_cause(cause);
  dbus::MessageWriter(&signal).AppendProtoAsArrayOfBytes(proto);
  dbus_wrapper->EmitSignal(&signal);
}

}  // namespace policy
}  // namespace power_manager
