// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_BUFFET_COMMAND_HANDLERS_H_
#define POWER_MANAGER_POWERD_BUFFET_COMMAND_HANDLERS_H_

#include <base/callback_forward.h>
#include <dbus/bus.h>

namespace power_manager {
namespace buffet {

// Initializes the GCD/Buffet command handler for 'base.reboot' command.
// |reboot_callback| is the callback that will be invoked when the reboot
// command is received from Buffet.
void InitCommandHandlers(const scoped_refptr<dbus::Bus>& bus,
                         const base::Closure& reboot_callback);

}  // namespace buffet
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_BUFFET_COMMAND_HANDLERS_H_
