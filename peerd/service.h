// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_SERVICE_H_
#define PEERD_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <base/macros.h>
#include <brillo/dbus/dbus_object.h>
#include <brillo/dbus/exported_property_set.h>
#include <brillo/errors/error.h>

#include "peerd/org.chromium.peerd.Service.h"
#include "peerd/typedefs.h"

namespace peerd {

namespace errors {
namespace service {

extern const char kInvalidServiceId[];
extern const char kInvalidServiceInfo[];
extern const char kInvalidServiceOptions[];

}  // namespace service
}  // namespace errors

// Exposes a Service over DBus.  This class is used to represent
// services exposed by the local device as well as remote devices.
class Service : public org::chromium::peerd::ServiceInterface {
 public:
  using ServiceInfo = std::map<std::string, std::string>;
  using IpAddress = std::tuple<std::vector<uint8_t>, uint16_t>;
  using IpAddresses = std::vector<IpAddress>;
  struct MDnsOptions {
    uint16_t port = 0;
  };

  static bool IsValidServiceId(brillo::ErrorPtr* error,
                               const std::string& service_id);
  static bool IsValidServiceInfo(brillo::ErrorPtr* error,
                                 const ServiceInfo& service_info);

  // For mDNS we'll encode each key/value pair as an entry in the TXT
  // record.  The max length there is 254 bytes + 1 byte to encode the
  // length of the key/value.
  static const size_t kMaxServiceInfoPairLength = 254;
  // mDNS forbids service types longer than 15 characters.
  static const size_t kMaxServiceIdLength = 15;

  Service(const scoped_refptr<dbus::Bus>& bus,
          brillo::dbus_utils::ExportedObjectManager* object_manager,
          const dbus::ObjectPath& path);
  bool RegisterAsync(
      brillo::ErrorPtr* error,
      const std::string& peer_id,
      const std::string& service_id,
      const IpAddresses& addresses,
      const ServiceInfo& service_info,
      const std::map<std::string, brillo::Any>& options,
      const CompletionAction& completion_callback);
  virtual ~Service() = default;

  // Getters called by publishers.
  std::string GetServiceId() const;
  IpAddresses GetIpAddresses() const;
  ServiceInfo GetServiceInfo() const;
  const MDnsOptions& GetMDnsOptions() const;

  // Update fields of this service.  If any field is found to be invalid, the
  // entire update is discarded.  Returns true if update is applied.
  bool Update(brillo::ErrorPtr* error,
              const IpAddresses& addresses,
              const ServiceInfo& info,
              const std::map<std::string, brillo::Any>& options);

 private:
  // Parses options for services being published by this device.
  bool ParseOptions(brillo::ErrorPtr* error,
                    const std::map<std::string, brillo::Any>& options,
                    MDnsOptions* mdns_options_out);
  // Checks that |maybe_mdns_options| is a map<string, Any> and then removes
  // values from that dictionary.  Stores the appropriate parsed values into
  // |mdns_options_|.
  bool ExtractMDnsOptions(brillo::ErrorPtr* error,
                          brillo::Any* maybe_mdns_options,
                          MDnsOptions* mdns_options_out);


  org::chromium::peerd::ServiceAdaptor dbus_adaptor_{this};
  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;
  MDnsOptions parsed_mdns_options_;

  friend class AvahiServicePublisherTest;
  friend class ServiceTest;
  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace peerd

#endif  // PEERD_SERVICE_H_
