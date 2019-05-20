// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CHROMEOS_IPCONFIG_DBUS_ADAPTOR_H_
#define SHILL_DBUS_CHROMEOS_IPCONFIG_DBUS_ADAPTOR_H_

#include <string>
#include <vector>

#include <base/macros.h>

#include "dbus_bindings/org.chromium.flimflam.IPConfig.h"
#include "shill/adaptor_interfaces.h"
#include "shill/dbus/chromeos_dbus_adaptor.h"

namespace shill {

class IPConfig;

// Subclass of DBusAdaptor for IPConfig objects
// There is a 1:1 mapping between IPConfig and ChromeosIPConfigDBusAdaptor
// instances.  Furthermore, the IPConfig owns the ChromeosIPConfigDBusAdaptor
// and manages its lifetime, so we're OK with ChromeosIPConfigDBusAdaptor
// having a bare pointer to its owner ipconfig.
class ChromeosIPConfigDBusAdaptor
    : public org::chromium::flimflam::IPConfigAdaptor,
      public org::chromium::flimflam::IPConfigInterface,
      public ChromeosDBusAdaptor,
      public IPConfigAdaptorInterface {
 public:
  static const char kInterfaceName[];
  static const char kPath[];

  ChromeosIPConfigDBusAdaptor(const scoped_refptr<dbus::Bus>& bus,
                              IPConfig* ipconfig);
  ~ChromeosIPConfigDBusAdaptor() override;

  // Implementation of IPConfigAdaptorInterface.
  const RpcIdentifier& GetRpcIdentifier() const override {
    return dbus_path();
  }
  void EmitBoolChanged(const std::string& name, bool value) override;
  void EmitUintChanged(const std::string& name, uint32_t value) override;
  void EmitIntChanged(const std::string& name, int value) override;
  void EmitStringChanged(const std::string& name,
                         const std::string& value) override;
  void EmitStringsChanged(const std::string& name,
                          const std::vector<std::string>& value) override;

  // Implementation of IPConfigAdaptor
  bool GetProperties(brillo::ErrorPtr* error,
                     brillo::VariantDictionary* properties) override;
  bool SetProperty(brillo::ErrorPtr* error,
                   const std::string& name,
                   const brillo::Any& value) override;
  bool ClearProperty(brillo::ErrorPtr* error,
                     const std::string& name) override;
  bool Remove(brillo::ErrorPtr* error) override;
  bool Refresh(brillo::ErrorPtr* error) override;

 private:
  IPConfig* ipconfig_;
  DISALLOW_COPY_AND_ASSIGN(ChromeosIPConfigDBusAdaptor);
};

}  // namespace shill

#endif  // SHILL_DBUS_CHROMEOS_IPCONFIG_DBUS_ADAPTOR_H_
