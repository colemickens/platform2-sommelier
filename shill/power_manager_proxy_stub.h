// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_POWER_MANAGER_PROXY_STUB_H_
#define SHILL_POWER_MANAGER_PROXY_STUB_H_

#include <string>

#include "shill/power_manager_proxy_interface.h"

namespace shill {

// Stub implementation for PowerManagerProxyInterface.
class PowerManagerProxyStub : public PowerManagerProxyInterface {
 public:
  PowerManagerProxyStub();
  ~PowerManagerProxyStub() override = default;

  // Inherited from PowerManagerProxyInterface.
  bool RegisterSuspendDelay(base::TimeDelta timeout,
                            const std::string& description,
                            int* delay_id_out) override;

  bool UnregisterSuspendDelay(int delay_id) override;

  bool ReportSuspendReadiness(int delay_id, int suspend_id) override;

  bool RegisterDarkSuspendDelay(base::TimeDelta timeout,
                                const std::string& description,
                                int* delay_id_out) override;

  bool UnregisterDarkSuspendDelay(int delay_id) override;

  bool ReportDarkSuspendReadiness(int delay_id, int suspend_id) override;

  bool RecordDarkResumeWakeReason(const std::string& wake_reason) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerManagerProxyStub);
};

}  // namespace shill

#endif  // SHILL_POWER_MANAGER_PROXY_STUB_H_
