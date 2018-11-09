// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_DBUS_SERVICE_DBUS_ADAPTOR_H_
#define APMANAGER_DBUS_SERVICE_DBUS_ADAPTOR_H_

#include <memory>
#include <string>

#include <base/macros.h>

#include <dbus_bindings/org.chromium.apmanager.Service.h>

#include "apmanager/error.h"
#include "apmanager/service_adaptor_interface.h"

namespace apmanager {

class Service;

class ServiceDBusAdaptor : public org::chromium::apmanager::ServiceInterface,
                           public ServiceAdaptorInterface {
 public:
  ServiceDBusAdaptor(const scoped_refptr<dbus::Bus>& bus,
                     brillo::dbus_utils::ExportedObjectManager* object_manager,
                     Service* service);
  ~ServiceDBusAdaptor() override;

  // Implementation of org::chromium::apmanager::ServiceInterface
  void Start(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>>
          response) override;
  bool Stop(brillo::ErrorPtr* dbus_error) override;

  // Implementation of ServiceAdaptorInterface.
  RPCObjectIdentifier GetRpcObjectIdentifier() override;
  void SetConfig(Config* config) override;
  void SetState(const std::string& state) override;

 private:
  void OnStartCompleted(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
      const Error& error);

  org::chromium::apmanager::ServiceAdaptor adaptor_;
  dbus::ObjectPath object_path_;
  brillo::dbus_utils::DBusObject dbus_object_;
  Service* service_;

  DISALLOW_COPY_AND_ASSIGN(ServiceDBusAdaptor);
};

}  // namespace apmanager

#endif  // APMANAGER_DBUS_SERVICE_DBUS_ADAPTOR_H_
