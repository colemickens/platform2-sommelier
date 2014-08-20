// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_COMMAND_DISPATCH_INTERFACE_H_
#define BUFFET_COMMANDS_COMMAND_DISPATCH_INTERFACE_H_

#include <string>

namespace buffet {

class CommandInstance;

// This is an abstract base interface that a command dispatcher will implement.
// It allows to abstract out the actual transport layer, such as D-Bus, from
// the rest of command registration and delivery subsystems.
class CommandDispachInterface {
 public:
  virtual ~CommandDispachInterface() = default;
  // Callback invoked by CommandQueue when a new command is added to the queue.
  virtual void OnCommandAdded(CommandInstance* command_instance) = 0;
  // Callback invoked by CommandQueue when a new command is removed from
  // the queue.
  virtual void OnCommandRemoved(CommandInstance* command_instance) = 0;
};

}  // namespace buffet

#endif  // BUFFET_COMMANDS_COMMAND_DISPATCH_INTERFACE_H_
