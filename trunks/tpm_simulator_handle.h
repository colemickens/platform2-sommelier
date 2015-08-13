// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TPM_SIMULATOR_HANDLE_H_
#define TRUNKS_TPM_SIMULATOR_HANDLE_H_

#include "trunks/command_transceiver.h"

#include <string>

#include "trunks/error_codes.h"

namespace trunks {

// Sends command requests to a software TPM via a handle to /dev/tpm-req.
// Receives command responses via a handle to /dev/tpm-resp. All commands are
// sent synchronously. The SendCommand method is supported but does not return
// until a response is received and the callback has been called. Command and
// response data are opaque to this class; it performs no validation.
//
// Example:
//   TpmSimulatorHandle handle;
//   if (!handle.Init()) {...}
//   std::string response = handle.SendCommandAndWait(command);
class TpmSimulatorHandle : public CommandTransceiver  {
 public:
  TpmSimulatorHandle();
  ~TpmSimulatorHandle() override;

  // Initializes a TpmSimulatorHandle instance. This method must be called
  // successfully before any other method. Returns true on success.
  bool Init() override;

  // CommandTranceiver methods.
  void SendCommand(const std::string& command,
                   const ResponseCallback& callback) override;
  std::string SendCommandAndWait(const std::string& command) override;

 private:
  // Writes a |command| to /dev/tpm-req and reads the |response| from
  // /dev/tpm-resp. Returns TPM_RC_SUCCESS on success.
  TPM_RC SendCommandInternal(const std::string& command, std::string* response);

  int req_fd_;  // A file descriptor for /dev/tpm-req.
  int resp_fd_;  // A file descriptor for /dev/tpm-resp.

  DISALLOW_COPY_AND_ASSIGN(TpmSimulatorHandle);
};

}  // namespace trunks

#endif  // TRUNKS_TPM_SIMULATOR_HANDLE_H_
