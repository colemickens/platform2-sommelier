// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CHROMEOS_UPSTART_PROXY_H_
#define SHILL_DBUS_CHROMEOS_UPSTART_PROXY_H_

#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>

#include "shill/upstart/upstart_proxy_interface.h"
#include "upstart/dbus-proxies.h"

namespace shill {

class ChromeosUpstartProxy : public UpstartProxyInterface {
 public:
  explicit ChromeosUpstartProxy(const scoped_refptr<dbus::Bus>& bus);
  ~ChromeosUpstartProxy() override = default;

  // Inherited from UpstartProxyInterface.
  void EmitEvent(const std::string& name,
                 const std::vector<std::string>& env,
                 bool wait) override;

 private:
  // Service path is provided in the xml file and will be populated by the
  // generator.
  static const char kServiceName[];

  // Callback for async call to EmitEvent.
  void OnEmitEventSuccess();
  void OnEmitEventFailure(brillo::Error* error);

  std::unique_ptr<com::ubuntu::Upstart0_6Proxy> upstart_proxy_;

  base::WeakPtrFactory<ChromeosUpstartProxy> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ChromeosUpstartProxy);
};

}  // namespace shill

#endif  // SHILL_DBUS_CHROMEOS_UPSTART_PROXY_H_
