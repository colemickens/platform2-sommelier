// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_AVAHI_SERVICE_PUBLISHER_H_
#define PEERD_AVAHI_SERVICE_PUBLISHER_H_

#include <map>
#include <stdint.h>
#include <string>
#include <vector>

#include <base/macros.h>
#include <chromeos/errors/error.h>
#include <gtest/gtest_prod.h>

#include "peerd/service.h"
#include "peerd/service_publisher_interface.h"

namespace dbus {
class ObjectPath;
}  // namespace dbus

namespace peerd {

namespace errors {
namespace avahi {

extern const char kRemovedUnknownService[];

}  // namespace avahi
}  // namespace errors

class AvahiServicePublisher : public ServicePublisherInterface {
 public:
  AvahiServicePublisher(const std::string& lan_name,
                        const scoped_refptr<dbus::Bus>& bus,
                        dbus::ObjectProxy* avahi_proxy);
  ~AvahiServicePublisher();
  base::WeakPtr<AvahiServicePublisher> GetWeakPtr();

  // See comments in ServicePublisherInterface.
  bool OnServiceUpdated(chromeos::ErrorPtr* error,
                        const Service& service) override;
  // See comments in ServicePublisherInterface.
  bool OnServiceRemoved(chromeos::ErrorPtr* error,
                        const std::string& service_id) override;

 private:
  using TxtRecord = std::vector<std::vector<uint8_t>>;
  // Transform a service_id to a mDNS compatible service type.
  static std::string GetServiceType(const Service& service);
  // Transform a service_info to a mDNS compatible TXT record value.
  // Concretely, a TXT record consists of a list of strings in the format
  // "key=value".  Each string must be less than 256 bytes long, since they are
  // length/value encoded.  Keys may not contain '=' characters, but are
  // otherwise unconstrained.
  //
  // We need a DBus type of "aay", which is a vector<vector<uint8_t>> in our
  // bindings.
  static TxtRecord GetTxtRecord(const Service& service);

  // Attempts to add the given |service| to the given |group_proxy|.
  // Returns true on success, false otherwise.  Does no cleanup.
  bool AddServiceToGroup(chromeos::ErrorPtr* error,
                         const Service& service,
                         dbus::ObjectProxy* group_proxy);
  // Removes all records corresponding to the provided |group_proxy| and
  // detaches from any related signals.
  bool FreeGroup(chromeos::ErrorPtr* error, dbus::ObjectProxy* group_proxy);

  const std::string lan_unique_hostname_;
  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* avahi_proxy_;
  std::map<std::string, dbus::ObjectProxy*> outstanding_groups_;

  // Should be last member to invalidate weak pointers in child objects
  // (like avahi_proxy_) and avoid callbacks while partially destroyed.
  base::WeakPtrFactory<AvahiServicePublisher> weak_ptr_factory_{this};
  FRIEND_TEST(AvahiServicePublisherTest, CanEncodeTxtRecords);
  DISALLOW_COPY_AND_ASSIGN(AvahiServicePublisher);
};

}  // namespace peerd

#endif  // PEERD_AVAHI_SERVICE_PUBLISHER_H_
