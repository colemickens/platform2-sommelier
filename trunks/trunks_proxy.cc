// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/bind.h>

#include "trunks/dbus_interface.h"
#include "trunks/trunks_proxy.h"

namespace trunks {

TrunksProxy::TrunksProxy() {}

TrunksProxy::~TrunksProxy() {}

bool TrunksProxy::Init() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  bus_ = new dbus::Bus(options);
  if (!bus_)
    return false;
  object_ = bus_->GetObjectProxy(
      trunks::kTrunksServiceName,
      dbus::ObjectPath(trunks::kTrunksServicePath));
  if (!object_)
    return false;
  return true;
}

void TrunksProxy::SendCommand(const std::string& command,
                              const SendCommandCallback& callback) {
  dbus::MethodCall method_call(trunks::kTrunksInterface, trunks::kSendCommand);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(command);
  object_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                      base::Bind(&trunks::TrunksProxy::OnResponse, callback));
}

void TrunksProxy::OnResponse(const SendCommandCallback& callback,
                             dbus::Response* response) {
  if (!response) {
    LOG(INFO) << "No response seen.";
    callback.Run("");
    return;
  }
  dbus::MessageReader reader(response);
  std::string value;
  reader.PopString(&value);
  callback.Run(value);
}

}  // namespace trunks

