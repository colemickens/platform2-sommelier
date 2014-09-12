// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_LIBBUFFET_COMMAND_H_
#define BUFFET_LIBBUFFET_COMMAND_H_

#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <chromeos/any.h>
#include <chromeos/dbus/data_serialization.h>

#include "libbuffet/export.h"

namespace dbus {
class ObjectProxy;
}

namespace buffet {

class CommandListener;
class CommandPropertySet;

// buffet::Command is a proxy class for GCD CommandInstance object delivered to
// command handling daemon over D-Bus.
class LIBBUFFET_EXPORT Command {
 public:
  Command(const dbus::ObjectPath& object_path,
          CommandListener* command_listener);

  // Returns the command ID.
  const std::string& GetID() const;
  // Returns the full name of the command.
  const std::string& GetName() const;
  // Returns the command category.
  const std::string& GetCategory() const;
  // Returns the command parameters and their values.
  const chromeos::dbus_utils::Dictionary& GetParameters() const;

  // Updates the command execution progress. The |progress| must be between
  // 0 and 100. Returns false if the progress value is incorrect.
  void SetProgress(int progress);
  // Aborts command execution.
  void Abort();
  // Cancels command execution.
  void Cancel();
  // Marks the command as completed successfully.
  void Done();

  // Command state getters.
  int GetProgress() const;
  const std::string& GetStatus() const;

 private:
  CommandPropertySet* GetProperties() const;
  dbus::ObjectProxy* GetObjectProxy() const;

  dbus::ObjectPath object_path_;
  CommandListener* command_listener_;

  DISALLOW_COPY_AND_ASSIGN(Command);
};

}  // namespace buffet

#endif  // BUFFET_LIBBUFFET_COMMAND_H_
