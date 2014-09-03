// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_COMMAND_PROXY_INTERFACE_H_
#define BUFFET_COMMANDS_COMMAND_PROXY_INTERFACE_H_

#include <string>

namespace buffet {

// This interface lets the command instance to update its proxy of command
// state changes, so that the proxy can then notify clients of the changes over
// their supported protocol (e.g. D-Bus).
class CommandProxyInterface {
 public:
  virtual ~CommandProxyInterface() = default;

  virtual void OnStatusChanged(const std::string& status) = 0;
  virtual void OnProgressChanged(int progress) = 0;
};

}  // namespace buffet

#endif  // BUFFET_COMMANDS_COMMAND_PROXY_INTERFACE_H_
