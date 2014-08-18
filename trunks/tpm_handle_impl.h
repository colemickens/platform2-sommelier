// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_TPM_HANDLE_IMPL_H_
#define TRUNKS_TPM_HANDLE_IMPL_H_

#include <stdint.h>
#include <string>

#include "trunks/error_codes.h"
#include "trunks/tpm_handle.h"

namespace trunks {

class TpmHandleImpl: public TpmHandle  {
 public:
  TpmHandleImpl();
  virtual ~TpmHandleImpl();
  virtual TPM_RC Init();
  virtual TPM_RC SendCommand(const std::string command, std::string* response);

 private:
  int fd_;
};

}  // namespace trunks

#endif  // TRUNKS_TPM_HANDLE_IMPL_H_
