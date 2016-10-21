//
// Copyright (C) 2016 The Android Open Source Project
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

#ifndef SHILL_DBUS_CHROMEOS_MM1_MODEM_LOCATION_PROXY_H_
#define SHILL_DBUS_CHROMEOS_MM1_MODEM_LOCATION_PROXY_H_

#include <string>
#include <vector>

#include "cellular/dbus-proxies.h"
#include "shill/cellular/mm1_modem_location_proxy_interface.h"

namespace shill {
namespace mm1 {

// A proxy to org.freedesktop.ModemManager1.Modem.Location.
class ChromeosModemLocationProxy : public ModemLocationProxyInterface {
 public:
  // Constructs an org.freedesktop.ModemManager1.Modem.Location DBus
  // object proxy at |path| owned by |service|.
  ChromeosModemLocationProxy(const scoped_refptr<dbus::Bus>& bus,
                             const std::string& path,
                             const std::string& service);
  ~ChromeosModemLocationProxy() override;
  // Inherited methods from ModemLocationProxyInterface.
  void Setup(uint32_t sources,
             bool signal_location,
             Error* error,
             const ResultCallback& callback,
             int timeout) override;

  void GetLocation(Error* error,
                   const BrilloAnyCallback& callback,
                   int timeout) override;

 private:
  // Callbacks for Setup async call.
  void OnSetupSuccess(const ResultCallback& callback);
  void OnSetupFailure(const ResultCallback& callback,
                      brillo::Error* dbus_error);

  // Callbacks for GetLocation async call.
  void OnGetLocationSuccess(const BrilloAnyCallback& callback,
                            const std::map<uint32_t, brillo::Any>& results);
  void OnGetLocationFailure(const BrilloAnyCallback& callback,
                            brillo::Error* dbus_error);

  std::unique_ptr<org::freedesktop::ModemManager1::Modem::LocationProxy> proxy_;

  base::WeakPtrFactory<ChromeosModemLocationProxy> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ChromeosModemLocationProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_DBUS_CHROMEOS_MM1_MODEM_LOCATION_PROXY_H_
