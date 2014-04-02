// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/dbus_manager.h"

#include <string>

#include <base/bind.h>
#include <dbus/object_path.h>

#include "buffet/async_event_sequencer.h"
#include "buffet/dbus_constants.h"
#include "buffet/dbus_utils.h"

using ::std::string;

namespace buffet {

DBusManager::DBusManager(dbus::Bus* bus)
    : bus_(bus),
      exported_object_(bus->GetExportedObject(
          dbus::ObjectPath(dbus_constants::kRootServicePath))) { }

DBusManager::~DBusManager() {
  // Unregister ourselves from the Bus.  This prevents the bus from calling
  // our callbacks in between the Manager's death and the bus unregistering
  // our exported object on shutdown.  Unretained makes no promises of memory
  // management.
  exported_object_->Unregister();
  exported_object_ = nullptr;
}

void DBusManager::Init(const OnInitFinish& cb) {
  scoped_refptr<dbus_utils::AsyncEventSequencer> sequencer(
      new dbus_utils::AsyncEventSequencer());
  exported_object_->ExportMethod(
      dbus_constants::kRootInterface, dbus_constants::kRootTestMethod,
      dbus_utils::GetExportableDBusMethod(
          base::Bind(&DBusManager::HandleTestMethod, base::Unretained(this))),
      sequencer->GetExportHandler(
          dbus_constants::kRootInterface, dbus_constants::kRootTestMethod,
          "Failed exporting DBusManager's test method",
          true));
  sequencer->OnAllTasksCompletedCall({cb});
}

scoped_ptr<dbus::Response> DBusManager::HandleTestMethod(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "Received call to test method.";
  return scoped_ptr<dbus::Response>();
}

}  // namespace buffet
