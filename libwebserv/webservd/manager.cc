// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libwebserv/webservd/manager.h"

#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::DBusObject;
using chromeos::dbus_utils::ExportedObjectManager;

namespace webservd {

Manager::Manager(ExportedObjectManager* object_manager)
  : dbus_object_{new DBusObject{
        object_manager, object_manager->GetBus(),
        org::chromium::WebServer::ManagerAdaptor::GetObjectPath()}} {}

void Manager::RegisterAsync(
    const AsyncEventSequencer::CompletionAction& completion_callback) {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  dbus_adaptor_.RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(
      sequencer->GetHandler("Failed exporting Manager.", true));
  sequencer->OnAllTasksCompletedCall({completion_callback});
}

std::string Manager::Ping() {
  return "Web Server is running";
}

}  // namespace webservd
