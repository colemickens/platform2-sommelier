// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_SERVICE_H_
#define PEERD_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include <base/macros.h>
#include <chromeos/dbus/dbus_object.h>
#include <chromeos/dbus/exported_property_set.h>
#include <chromeos/errors/error.h>

#include "peerd/dbus_data_serialization.h"
#include "peerd/ip_addr.h"
#include "peerd/typedefs.h"

namespace peerd {

namespace errors {
namespace service {

extern const char kInvalidServiceId[];
extern const char kInvalidServiceInfo[];

}  // namespace service
}  // namespace errors

// Exposes a Service over DBus.  This class is used to represent
// services exposed by the local device as well as remote devices.
class Service {
 public:
  using ServiceInfo = std::map<std::string, std::string>;
  using IpAddresses = std::vector<ip_addr>;

  static const size_t kMaxServiceInfoPairLength = 254;
  static const size_t kMaxServiceIdLength = 31;

  // Construct a Service object and register it with DBus on the
  // provided |path|.  On error, nullptr is returned, and |error| is
  // populated with meaningful error information.
  static std::unique_ptr<Service> MakeService(
      chromeos::ErrorPtr* error,
      chromeos::dbus_utils::ExportedObjectManager* object_manager,
      const dbus::ObjectPath& path,
      const std::string& service_id,
      const IpAddresses& addresses,
      const ServiceInfo& service_info,
      const CompletionAction& completion_callback);

  virtual ~Service() = default;

 private:
  // Used for testing, where we want to use a MockDBusObject.
  static std::unique_ptr<Service> MakeServiceImpl(
      chromeos::ErrorPtr* error,
      std::unique_ptr<chromeos::dbus_utils::DBusObject> dbus_object,
      const std::string& service_id,
      const IpAddresses& addresses,
      const ServiceInfo& service_info,
      const CompletionAction& completion_callback);

  static bool IsValidServiceId(chromeos::ErrorPtr* error,
                               const std::string& service_id);
  static bool IsValidServiceInfo(chromeos::ErrorPtr* error,
                                 const ServiceInfo& service_info);

  Service(std::unique_ptr<chromeos::dbus_utils::DBusObject> dbus_object,
          const std::string& service_id,
          const IpAddresses& addresses,
          const ServiceInfo& service_info);
  void RegisterAsync(const CompletionAction& completion_callback);

  chromeos::dbus_utils::ExportedProperty<std::string> service_id_;
  chromeos::dbus_utils::ExportedProperty<IpAddresses> ip_addresses_;
  chromeos::dbus_utils::ExportedProperty<ServiceInfo> service_info_;
  std::unique_ptr<chromeos::dbus_utils::DBusObject> dbus_object_;

  friend class ServiceTest;
  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace peerd

#endif  // PEERD_SERVICE_H_
