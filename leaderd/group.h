// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LEADERD_GROUP_H_
#define LEADERD_GROUP_H_

#include <string>

#include <base/macros.h>
#include <chromeos/dbus/async_event_sequencer.h>

#include "leaderd/org.chromium.leaderd.Group.h"

namespace chromeos {
namespace dbus_utils {
class ExportedObjectManager;
}  // dbus_utils
}  // chromeos

namespace leaderd {

class Manager;

// Represents a single group advertisement.
class Group : public org::chromium::leaderd::GroupInterface {
 public:
  using CompletionAction =
      chromeos::dbus_utils::AsyncEventSequencer::CompletionAction;

  Group(const std::string& guid, const scoped_refptr<dbus::Bus>& bus,
        chromeos::dbus_utils::ExportedObjectManager* object_manager,
        const dbus::ObjectPath& path, Manager* manager);
  ~Group() override;
  void RegisterAsync(const CompletionAction& completion_callback);
  const dbus::ObjectPath& GetObjectPath() const;

  // DBus handlers
  bool LeaveGroup(chromeos::ErrorPtr* error) override;
  bool SetScore(chromeos::ErrorPtr* error, int32_t in_score) override;
  bool PokeLeader(chromeos::ErrorPtr* error) override;

 private:
  const std::string guid_;
  const dbus::ObjectPath object_path_;
  const Manager* manager_;

  org::chromium::leaderd::GroupAdaptor dbus_adaptor_{this};
  chromeos::dbus_utils::DBusObject dbus_object_;

  DISALLOW_COPY_AND_ASSIGN(Group);
};

}  // namespace leaderd

#endif  // LEADERD_GROUP_H_
