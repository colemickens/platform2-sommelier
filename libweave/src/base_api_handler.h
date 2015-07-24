// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_BASE_API_HANDLER_H_
#define LIBWEAVE_SRC_BASE_API_HANDLER_H_

#include <memory>
#include <string>

#include <base/memory/weak_ptr.h>

namespace weave {

class Command;
class CommandManager;
class DeviceRegistrationInfo;
class StateManager;

// Handles commands from 'base' package.
// Objects of the class subscribe for notification from CommandManager and
// execute incoming commands.
// Handled commands:
//  base.updateDeviceInfo
//  base.updateBaseConfiguration
class BaseApiHandler final {
 public:
  BaseApiHandler(const base::WeakPtr<DeviceRegistrationInfo>& device_info,
                 const std::shared_ptr<StateManager>& state_manager,
                 const std::shared_ptr<CommandManager>& command_manager);

 private:
  void OnCommandAdded(Command* command);
  void UpdateBaseConfiguration(Command* command);
  void UpdateDeviceInfo(Command* command);

  base::WeakPtr<DeviceRegistrationInfo> device_info_;
  std::shared_ptr<StateManager> state_manager_;

  base::WeakPtrFactory<BaseApiHandler> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(BaseApiHandler);
};

}  // namespace weave

#endif  // LIBWEAVE_SRC_BASE_API_HANDLER_H_
