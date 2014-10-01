// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_COMMAND_TRANSCEIVER_H_
#define TRUNKS_COMMAND_TRANSCEIVER_H_

#include <string>

#include <base/callback_forward.h>

namespace trunks {

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
      const base::Callback<void(const std::string& response)>& callback) = 0;

  // Sends |command| unmodified to a TPM device and waits for the |response|. If
  // a transmission error occurs a TCTI layer error code is returned. On success
  // returns TPM_RC_SUCCESS.
  virtual uint32_t SendCommandAndWait(const std::string& command,
                                      std::string* response) = 0;
};

}  // namespace trunks

#endif  // TRUNKS_COMMAND_TRANSCEIVER_H_
