// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/manager_dbus_proxy.h"

#include <chromeos/async_event_sequencer.h>
#include <chromeos/dbus_utils.h>
#include <dbus/message.h>
#include <dbus/object_path.h>

#include "peerd/dbus_constants.h"
#include "peerd/manager.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::GetBadArgsError;
using dbus::ExportedObject;
using dbus::MessageReader;
using dbus::MethodCall;
using dbus::ObjectPath;
using peerd::dbus_constants::kErrorTooManyParameters;
using peerd::dbus_constants::kManagerInterface;
using peerd::dbus_constants::kManagerPing;
using peerd::dbus_constants::kManagerServicePath;

namespace peerd {

ManagerDBusProxy::ManagerDBusProxy(const scoped_refptr<dbus::Bus>& bus,
                                   Manager* manager)
  : bus_(bus),
    exported_object_(bus->GetExportedObject(ObjectPath(kManagerServicePath))),
    manager_(manager) {
}

ManagerDBusProxy::~ManagerDBusProxy() {
  exported_object_->Unregister();
  exported_object_ = nullptr;
}

void ManagerDBusProxy::Init(const OnInitFinish& success_cb) {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  ExportedObject::OnExportedCallback on_export_cb;
  on_export_cb = sequencer->GetExportHandler(
      kManagerInterface, kManagerPing,
      "Failed exporting Manager.Ping method", true);
  exported_object_->ExportMethod(
      kManagerInterface, kManagerPing,
      base::Bind(&ManagerDBusProxy::HandlePing, base::Unretained(this)),
      on_export_cb);
  sequencer->OnAllTasksCompletedCall({success_cb});
}

void ManagerDBusProxy::HandlePing(
    MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  VLOG(1) << "Manager.HandlePing called.";
  // Read the parameters to the method.
  MessageReader reader(method_call);
  if (reader.HasMoreData()) {
    response_sender.Run(GetBadArgsError(method_call, kErrorTooManyParameters));
    return;
  }
  scoped_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(method_call));
  dbus::MessageWriter writer(response.get());
  writer.AppendString(manager_->Ping());
  response_sender.Run(response.Pass());
}

}  // namespace peerd
