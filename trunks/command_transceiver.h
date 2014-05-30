// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_COMMAND_TRANSCEIVER_H_
#define TRUNKS_COMMAND_TRANSCEIVER_H_

#include <string>

namespace Trunks {

// CommandTransceiver is an interface that sends commands to a TPM device and
// receives responses asynchronously.
class CommandTransceiver {
 public:
  // Sends |command| unmodified to a TPM device.  When a response is received
  // |callback| will be called with the unmodified |response| from the device.
  // If a transmission error occurs a response message is created and sent to
  // the callback using an error code defined by the TCG for the TCTI layer.
  virtual void SendCommand(
      const std::string& command,
      base::Callback<void(const std::string& response)> callback) = 0;
};

}  // namespace Trunks

#endif  // TRUNKS_COMMAND_TRANSCEIVER_H_
