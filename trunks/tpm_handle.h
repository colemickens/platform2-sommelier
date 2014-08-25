// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TPM_HANDLE_H_
#define TRUNKS_TPM_HANDLE_H_

#include <stdint.h>
#include <string>

namespace trunks {

// This class is instantiated in the trunks daemon and is
// the interface to talk to the tpm. It opens the tpm file
// descriptor at "/dev/tpm0". Using the |SendCommand| method
// trunksd can write commands to the tpm and read its responses.
// To use the class, we have to instantiate it, and call |Init|
// before sending any commands.
class TpmHandle {
 public:
  // This method opens the TPM file descriptor in read/write mode.
  // This method can fail to open "/dev/tpm0" and return
  // TCTI_RC_GENERAL_FAILURE. Returns TPM_RC_SUCCESS on success.
  virtual TPM_RC Init() = 0;
  // This method allows us to write a command to the TPM.
  // The first parameter |command| is the command to be written
  // to the TPM. This is written to the TPM file descriptor.
  // The response is then read and written to the output string
  // at |response|.
  virtual TPM_RC SendCommand(const std::string& command,
                             std::string* response) = 0;
};

}  // namespace trunks

#endif  // TRUNKS_TPM_HANDLE_H_
