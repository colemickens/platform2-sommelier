//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef SHILL_POWER_MANAGER_PROXY_H_
#define SHILL_POWER_MANAGER_PROXY_H_

// An implementation of PowerManagerProxyInterface.  It connects to the dbus and
// listens for events from the power manager.  When they occur, the delegate's
// member functions are called.

#include <stdint.h>

#include <string>
#include <vector>

#include <base/compiler_specific.h>

#include "shill/dbus_proxies/power_manager.h"
#include "shill/power_manager_proxy_interface.h"

namespace shill {

class PowerManagerProxy : public PowerManagerProxyInterface {
 public:
  // Constructs a PowerManager DBus object proxy with signals dispatched to
  // |delegate|.
  PowerManagerProxy(PowerManagerProxyDelegate* delegate,
                    DBus::Connection* connection);
  ~PowerManagerProxy() override;

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
  class Proxy : public org::chromium::PowerManager_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(PowerManagerProxyDelegate* delegate,
          DBus::Connection* connection);
    ~Proxy() override;

   private:
    // Signal callbacks inherited from org::chromium::PowerManager_proxy.
    void SuspendImminent(const std::vector<uint8_t>& serialized_proto) override;
    void SuspendDone(const std::vector<uint8_t>& serialized_proto) override;
    void DarkSuspendImminent(
        const std::vector<uint8_t>& serialized_proto) override;

    PowerManagerProxyDelegate* const delegate_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  bool RegisterSuspendDelayInternal(bool is_dark,
                                    base::TimeDelta timeout,
                                    const std::string& description,
                                    int* delay_id_out);
  bool UnregisterSuspendDelayInternal(bool is_dark, int delay_id);
  bool ReportSuspendReadinessInternal(bool is_dark,
                                      int delay_id,
                                      int suspend_id);

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(PowerManagerProxy);
};

}  // namespace shill

#endif  // SHILL_POWER_MANAGER_PROXY_H_
