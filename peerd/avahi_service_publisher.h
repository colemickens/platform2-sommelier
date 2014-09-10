// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_AVAHI_SERVICE_PUBLISHER_H_
#define PEERD_AVAHI_SERVICE_PUBLISHER_H_

#include <map>
#include <string>

#include <chromeos/errors/error.h>

#include "peerd/service.h"
#include "peerd/service_publisher_interface.h"

namespace dbus {
class ObjectPath;
}  // namespace dbus

namespace peerd {

class AvahiServicePublisher : public ServicePublisherInterface {
 public:
  AvahiServicePublisher(const scoped_refptr<dbus::Bus>& bus,
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
  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* avahi_proxy_;
  std::map<std::string, dbus::ObjectProxy*> outstanding_groups_;

  // Should be last member to invalidate weak pointers in child objects
  // (like avahi_proxy_) and avoid callbacks while partially destroyed.
  base::WeakPtrFactory<AvahiServicePublisher> weak_ptr_factory_{this};
};

}  // namespace peerd

#endif  // PEERD_AVAHI_SERVICE_PUBLISHER_H_
