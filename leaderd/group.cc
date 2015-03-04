// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leaderd/group.h"

#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <chromeos/errors/error.h>
#include <dbus/bus.h>

#include "leaderd/manager.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::DBusObject;
using chromeos::dbus_utils::ExportedObjectManager;

namespace leaderd {

Group::Group(const std::string& guid, const scoped_refptr<dbus::Bus>& bus,
             ExportedObjectManager* object_manager,
             const dbus::ObjectPath& path, Manager* manager)
    : guid_(guid),
      object_path_(path),
      manager_(manager),
      dbus_object_(object_manager, bus, path) {}

Group::~Group() {}

void Group::RegisterAsync(const CompletionAction& completion_callback) {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  dbus_adaptor_.RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(
      sequencer->GetHandler("Failed exporting DBusManager.", true));
  sequencer->OnAllTasksCompletedCall({completion_callback});
}

const dbus::ObjectPath& Group::GetObjectPath() const { return object_path_; }

bool Group::LeaveGroup(chromeos::ErrorPtr* error) { return true; }

bool Group::SetScore(chromeos::ErrorPtr* error, int32_t in_score) {
  return true;
}

void Group::ChallengeLeader(const std::string& uuid, int score,
                            std::string* leader, std::string* my_uuid) {}

bool Group::PokeLeader(chromeos::ErrorPtr* error) { return true; }

}  // namespace leaderd
