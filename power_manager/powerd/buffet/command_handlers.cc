// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/buffet/command_handlers.h"

#include <memory>

#include <base/callback.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>

#include "buffet/dbus-proxies.h"

using org::chromium::Buffet::CommandProxy;
using org::chromium::Buffet::ObjectManagerProxy;

namespace power_manager {
namespace buffet {

namespace {

// The GCD command name powerd handles.
const char kBaseRebootCommand[] = "base.reboot";

// The command status "done" is ignored, which means this command has already
// been processed.
const char kCommandStatusDone[] = "done";

// A helper class to register callbacks with Buffet to be notified of new
// GCD commands coming in.
class CommandHandler final {
 public:
  CommandHandler() : weak_ptr_factory_(this) {}

  // Initialize the object and start listening for Buffet commands on D-Bus.
  void Start(const scoped_refptr<dbus::Bus>& bus,
             const base::Closure& reboot_callback) {
    reboot_callback_ = reboot_callback;
    object_manager_.reset(new ObjectManagerProxy(bus));
    object_manager_->SetCommandAddedCallback(
        base::Bind(&CommandHandler::OnCommand, weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Command handler callback method. Called when a new command is available
  // on D-Bus from Buffet.
  void OnCommand(CommandProxy* command) {
    if (command->status() != kCommandStatusDone &&
        command->name() == kBaseRebootCommand) {
      // Right now powerd handles only 'base.reboot' command and ignores
      // everything else.
      if (command->Done(nullptr))
        reboot_callback_.Run();
    }
}

  std::unique_ptr<ObjectManagerProxy> object_manager_;
  base::Closure reboot_callback_;

  base::WeakPtrFactory<CommandHandler> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(CommandHandler);
};

}  // anonymous namespace

void InitCommandHandlers(const scoped_refptr<dbus::Bus>& bus,
                         const base::Closure& reboot_callback) {
  static CommandHandler command_handler;
  command_handler.Start(bus, reboot_callback);
}

}  // namespace buffet
}  // namespace power_manager
