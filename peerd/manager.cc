// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/manager.h"

#include <base/bind.h>
#include <chromeos/async_event_sequencer.h>
#include <chromeos/dbus_utils.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>
#include <dbus/object_path.h>

#include "peerd/dbus_constants.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::GetBadArgsError;
using dbus::ExportedObject;
using peerd::dbus_constants::kManagerServicePath;

namespace peerd {

Manager::Manager(const scoped_refptr<dbus::Bus>& bus)
  : bus_(bus),
    exported_object_(bus->GetExportedObject(
        dbus::ObjectPath(kManagerServicePath))) {
}

Manager::~Manager() {
  exported_object_->Unregister();
  exported_object_ = nullptr;
}

void Manager::Init(const OnInitFinish& success_cb) {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  const ExportedObject::OnExportedCallback on_export_cb =
      sequencer->GetExportHandler(dbus_constants::kManagerInterface,
                                  dbus_constants::kManagerPing,
                                  "Failed exporting Manager.Ping method",
                                  true);
  exported_object_->ExportMethod(
      dbus_constants::kManagerInterface,
      dbus_constants::kManagerPing,
      base::Bind(&Manager::HandlePing, base::Unretained(this)),
      on_export_cb);
  sequencer->OnAllTasksCompletedCall({success_cb});
}

void Manager::HandlePing(dbus::MethodCall* method_call,
                         dbus::ExportedObject::ResponseSender response_sender) {
  VLOG(1) << "Manager.HandlePing called.";
  // Read the parameters to the method.
  dbus::MessageReader reader(method_call);
  if (reader.HasMoreData()) {
    response_sender.Run(
        GetBadArgsError(method_call, "Too many parameters to Ping"));
    return;
  }
  scoped_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(method_call));
  dbus::MessageWriter writer(response.get());
  writer.AppendString("Hello world!");
  response_sender.Run(response.Pass());
}

}  // namespace peerd
