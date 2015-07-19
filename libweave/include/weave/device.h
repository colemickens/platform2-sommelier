// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_DEVICE_H_
#define LIBWEAVE_INCLUDE_WEAVE_DEVICE_H_

#include <memory>
#include <set>
#include <string>

#include <base/files/file_path.h>
#include <chromeos/errors/error.h>
#include <chromeos/variant_dictionary.h>

#include "weave/types.h"

namespace chromeos {
namespace dbus_utils {
class AsyncEventSequencer;
class DBusObject;
}
}

namespace weave {

class CommandManager;
class StateManager;
class BuffetConfig;
class DeviceRegistrationInfo;
namespace privet {
class Manager;
}

// TODO(vitalybuka): Replace with interfaces.
using Commands = CommandManager;
using State = StateManager;
using Config = BuffetConfig;
using Cloud = DeviceRegistrationInfo;
using Privet = privet::Manager;

class Device {
 public:
  struct Options {
    base::FilePath config_path;
    base::FilePath state_path;
    base::FilePath definitions_path;
    base::FilePath test_definitions_path;
    bool xmpp_enabled = true;
    std::set<std::string> device_whitelist;
    bool disable_privet = false;
    bool disable_security = false;
    bool enable_ping = false;
    std::string test_privet_ssid;
  };

  virtual ~Device() = default;

  virtual void Start(const Options& options,
                     chromeos::dbus_utils::DBusObject* dbus_object,
                     chromeos::dbus_utils::AsyncEventSequencer* sequencer) = 0;

  virtual Commands* GetCommands() = 0;
  virtual State* GetState() = 0;
  virtual Config* GetConfig() = 0;
  virtual Cloud* GetCloud() = 0;
  virtual Privet* GetPrivet() = 0;

  static std::unique_ptr<Device> Create();
};

}  // namespace weave

// TODO(vitalybuka): Replace with interfaces
#include "libweave/src/buffet_config.h"
#include "libweave/src/commands/command_manager.h"
#include "libweave/src/device_registration_info.h"
#include "libweave/src/privet/privet_manager.h"
#include "libweave/src/states/state_manager.h"

#endif  // LIBWEAVE_INCLUDE_WEAVE_DEVICE_H_
