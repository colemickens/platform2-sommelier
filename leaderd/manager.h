// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LEADERD_MANAGER_H_
#define LEADERD_MANAGER_H_

#include <string>

#include <base/macros.h>
#include <chromeos/dbus/async_event_sequencer.h>

#include "leaderd/org.chromium.leaderd.Manager.h"

namespace chromeos {
namespace dbus_utils {
class ExportedObjectManager;
}  // dbus_utils
}  // chromeos

namespace leaderd {

// Manages global state of leaderd.
class Manager : public org::chromium::leaderd::ManagerInterface {
 public:
  using CompletionAction =
      chromeos::dbus_utils::AsyncEventSequencer::CompletionAction;

  explicit Manager(chromeos::dbus_utils::ExportedObjectManager* object_manager);
  ~Manager() override = default;
  void RegisterAsync(const CompletionAction& completion_callback);

  std::string Ping() override;

 private:
  org::chromium::leaderd::ManagerAdaptor dbus_adaptor_{this};
  std::unique_ptr<chromeos::dbus_utils::DBusObject> dbus_object_;

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace leaderd

#endif  // LEADERD_MANAGER_H_
