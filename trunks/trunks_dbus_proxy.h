//
// Copyright (C) 2014 The Android Open Source Project
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

#ifndef TRUNKS_TRUNKS_DBUS_PROXY_H_
#define TRUNKS_TRUNKS_DBUS_PROXY_H_

#include <string>

#include <base/memory/weak_ptr.h>
#include <base/threading/platform_thread.h>
#include <brillo/dbus/dbus_method_invoker.h>
#include <dbus/bus.h>
#include <dbus/object_proxy.h>

#include "trunks/command_transceiver.h"
#include "trunks/trunks_export.h"

namespace trunks {

// TrunksDBusProxy is a CommandTransceiver implementation that forwards all
// commands to the trunksd D-Bus daemon. See TrunksDBusService for details on
// how the commands are handled once they reach trunksd. A TrunksDBusProxy
// instance must be used in only one thread.
class TRUNKS_EXPORT TrunksDBusProxy : public CommandTransceiver {
 public:
  TrunksDBusProxy() = default;
  ~TrunksDBusProxy() override;

  // Initializes the D-Bus client. Returns true on success.
  bool Init() override;

  // CommandTransceiver methods.
  void SendCommand(const std::string& command,
                   const ResponseCallback& callback) override;
  std::string SendCommandAndWait(const std::string& command) override;

  // Returns the service readiness flag. Forces re-check for readiness if
  // the flag is not set or |force_check| is passed.
  bool IsServiceReady(bool force_check);

  void set_init_timeout(base::TimeDelta init_timeout) {
    init_timeout_ = init_timeout;
  }
  void set_init_attempt_delay(base::TimeDelta init_attempt_delay) {
    init_attempt_delay_ = init_attempt_delay;
  }

 private:
  friend class TrunksDBusProxyTest;

  // Constructor for mock bus injection in unit tests.
  explicit TrunksDBusProxy(dbus::Bus* bus) : bus_(bus) {}

  // Checks service readiness, i.e. that trunksd is registered on dbus.
  bool CheckIfServiceReady();

  // Handles errors received from dbus.
  void OnError(const ResponseCallback& callback, brillo::Error* error);

  base::WeakPtr<TrunksDBusProxy> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  bool service_ready_ = false;
  // Timeout waiting for trunksd service readiness on dbus when initializing.
  base::TimeDelta init_timeout_ = base::TimeDelta::FromSeconds(30);
  // Delay between subsequent checks if trunksd is ready on dbus.
  base::TimeDelta init_attempt_delay_ = base::TimeDelta::FromMilliseconds(300);

  base::PlatformThreadId origin_thread_id_;
  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* object_proxy_ = nullptr;

  // Declared last so weak pointers are invalidated first on destruction.
  base::WeakPtrFactory<TrunksDBusProxy> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TrunksDBusProxy);
};

}  // namespace trunks

#endif  // TRUNKS_TRUNKS_DBUS_PROXY_H_
