// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CHROMEOS_MODEM_GOBI_PROXY_H_
#define SHILL_DBUS_CHROMEOS_MODEM_GOBI_PROXY_H_

#include <string>

#include <base/macros.h>

#include "cellular/dbus-proxies.h"
#include "shill/cellular/modem_gobi_proxy_interface.h"

namespace shill {

// A proxy to (old) ModemManager.Modem.Gobi.
class ChromeosModemGobiProxy : public ModemGobiProxyInterface {
 public:
  // Constructs a ModemManager.Modem.Gobi DBus object proxy at |path| owned by
  // |service|.
  ChromeosModemGobiProxy(const scoped_refptr<dbus::Bus>& bus,
                         const std::string& path,
                         const std::string& service);
  ~ChromeosModemGobiProxy() override;

  // Inherited from ModemGobiProxyInterface.
  void SetCarrier(const std::string& carrier,
                  Error* error,
                  const ResultCallback& callback,
                  int timeout) override;

 private:
  // Callbacks for SetCarrier async call.
  void OnSetCarrierSuccess(const ResultCallback& callback);
  void OnSetCarrierFailure(const ResultCallback& callback,
                           chromeos::Error* dbus_error);

  std::unique_ptr<org::chromium::ModemManager::Modem::GobiProxy> proxy_;

  base::WeakPtrFactory<ChromeosModemGobiProxy> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ChromeosModemGobiProxy);
};

}  // namespace shill

#endif  // SHILL_DBUS_CHROMEOS_MODEM_GOBI_PROXY_H_
