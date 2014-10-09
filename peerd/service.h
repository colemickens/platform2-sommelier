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

namespace constants {
extern const char kSerbusServiceId[];
}  // namespace constants

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

  // For mDNS we'll encode each key/value pair as an entry in the TXT
  // record.  The max length there is 254 bytes + 1 byte to encode the
  // length of the key/value.
  static const size_t kMaxServiceInfoPairLength = 254;
  // mDNS forbids service types longer than 15 characters.
  static const size_t kMaxServiceIdLength = 15;

  Service(const scoped_refptr<dbus::Bus>& bus,
          chromeos::dbus_utils::ExportedObjectManager* object_manager,
          const dbus::ObjectPath& path);
  bool RegisterAsync(
      chromeos::ErrorPtr* error,
      const std::string& service_id,
      const IpAddresses& addresses,
      const ServiceInfo& service_info,
      const CompletionAction& completion_callback);
  virtual ~Service() = default;

  // Getters called by publishers.
  const std::string& GetServiceId() const;
  const IpAddresses& GetIpAddresses() const;
  const ServiceInfo& GetServiceInfo() const;

  // Update fields of this service.  If any field is found to be invalid, the
  // entire update is discarded.  Returns true if update is applied.
  bool Update(chromeos::ErrorPtr* error,
              const IpAddresses& addresses,
              const ServiceInfo& info);

 private:
  static bool IsValidServiceId(chromeos::ErrorPtr* error,
                               const std::string& service_id);
  static bool IsValidServiceInfo(chromeos::ErrorPtr* error,
                                 const ServiceInfo& service_info);

  chromeos::dbus_utils::ExportedProperty<std::string> service_id_;
  chromeos::dbus_utils::ExportedProperty<IpAddresses> ip_addresses_;
  chromeos::dbus_utils::ExportedProperty<ServiceInfo> service_info_;
  std::unique_ptr<chromeos::dbus_utils::DBusObject> dbus_object_;

  friend class AvahiServicePublisherTest;
  friend class ServiceTest;
  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace peerd

#endif  // PEERD_SERVICE_H_
