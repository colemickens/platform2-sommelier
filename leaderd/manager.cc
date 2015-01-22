// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leaderd/manager.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::DBusObject;
using chromeos::dbus_utils::ExportedObjectManager;
using org::chromium::leaderd::ManagerAdaptor;

namespace leaderd {

namespace {

const char kPingResponse[] = "Hello world!";

}  // namespace

Manager::Manager(ExportedObjectManager* object_manager)
    : dbus_object_{new DBusObject{object_manager,
                                  object_manager->GetBus(),
                                  ManagerAdaptor::GetObjectPath()}} {
}

void Manager::RegisterAsync(const CompletionAction& completion_callback) {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  dbus_adaptor_.RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(
      sequencer->GetHandler("Failed exporting Manager.", true));
  sequencer->OnAllTasksCompletedCall({completion_callback});
}

std::string Manager::Ping() {
  return kPingResponse;
}

}  // namespace leaderd
