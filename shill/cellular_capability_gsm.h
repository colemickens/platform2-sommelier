// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CAPABILITY_GSM_
#define SHILL_CELLULAR_CAPABILITY_GSM_

#include <base/task.h>

#include "shill/cellular_capability.h"

namespace shill {

class CellularCapabilityGSM : public CellularCapability {
 public:
  CellularCapabilityGSM(Cellular *cellular);

  // Inherited from CellularCapability.
  virtual void InitProxies();
  virtual void RequirePIN(const std::string &pin, bool require, Error *error);
  virtual void EnterPIN(const std::string &pin, Error *error);
  virtual void UnblockPIN(const std::string &unblock_code,
                          const std::string &pin,
                          Error *error);
  virtual void ChangePIN(const std::string &old_pin,
                         const std::string &new_pin,
                         Error *error);

 private:
  void RequirePINTask(const std::string &pin, bool require);
  void EnterPINTask(const std::string &pin);
  void UnblockPINTask(const std::string &unblock_code, const std::string &pin);
  void ChangePINTask(const std::string &old_pin, const std::string &new_pin);

  ScopedRunnableMethodFactory<CellularCapabilityGSM> task_factory_;

  DISALLOW_COPY_AND_ASSIGN(CellularCapabilityGSM);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CAPABILITY_GSM_
