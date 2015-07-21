// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_COMMANDS_COMMAND_PROXY_INTERFACE_H_
#define LIBWEAVE_SRC_COMMANDS_COMMAND_PROXY_INTERFACE_H_

#include <string>

#include "libweave/src/commands/schema_utils.h"

namespace weave {

// This interface lets the command instance to update its proxy of command
// state changes, so that the proxy can then notify clients of the changes over
// their supported protocol (e.g. D-Bus).
class CommandProxyInterface {
 public:
  virtual ~CommandProxyInterface() = default;

  virtual void OnResultsChanged() = 0;
  virtual void OnStatusChanged() = 0;
  virtual void OnProgressChanged() = 0;
  virtual void OnCommandDestroyed() = 0;
};

}  // namespace weave

#endif  // LIBWEAVE_SRC_COMMANDS_COMMAND_PROXY_INTERFACE_H_
