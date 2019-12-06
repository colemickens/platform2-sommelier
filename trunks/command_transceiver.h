// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_COMMAND_TRANSCEIVER_H_
#define TRUNKS_COMMAND_TRANSCEIVER_H_

#include <string>

#include <base/callback_forward.h>
#include <trunks/tpm_constants.h>

namespace trunks {

// CommandTransceiver is an interface that sends commands to a TPM device and
// receives responses. It can operate synchronously or asynchronously.
class CommandTransceiver {
 public:
  typedef base::Callback<void(const std::string& response)> ResponseCallback;

  virtual ~CommandTransceiver() {}

  // Sends a TPM |command| asynchronously. When a |response| is received,
  // |callback| will be called with the |response| data from the TPM. If a
  // transmission error occurs |callback| will be called with a well-formed
  // error |response|.
  virtual void SendCommand(const std::string& command,
                           const ResponseCallback& callback) = 0;

  // Sends a TPM |command| synchronously (i.e. waits for a response) and returns
  // the response. If a transmission error occurs the response will be populated
  // with a well-formed error response.
  virtual std::string SendCommandAndWait(const std::string& command) = 0;

  // Initializes the actual interface, replaced by the derived classes, where
  // needed.
  virtual bool Init() { return true; }
};

}  // namespace trunks

#endif  // TRUNKS_COMMAND_TRANSCEIVER_H_
