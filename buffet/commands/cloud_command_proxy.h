// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_CLOUD_COMMAND_PROXY_H_
#define BUFFET_COMMANDS_CLOUD_COMMAND_PROXY_H_

#include <base/macros.h>

#include <string>

#include "buffet/commands/command_proxy_interface.h"

namespace buffet {

class CommandInstance;
class DeviceRegistrationInfo;

// Command proxy which publishes command updates to the cloud.
class CloudCommandProxy final : public CommandProxyInterface {
 public:
  CloudCommandProxy(CommandInstance* command_instance,
                    DeviceRegistrationInfo* device_registration_info);
  ~CloudCommandProxy() override = default;

  // CommandProxyInterface implementation/overloads.
  void OnResultsChanged(const native_types::Object& results) override;
  void OnStatusChanged(const std::string& status) override;
  void OnProgressChanged(int progress) override;

 private:
  CommandInstance* command_instance_;
  DeviceRegistrationInfo* device_registration_info_;

  DISALLOW_COPY_AND_ASSIGN(CloudCommandProxy);
};

}  // namespace buffet

#endif  // BUFFET_COMMANDS_CLOUD_COMMAND_PROXY_H_
