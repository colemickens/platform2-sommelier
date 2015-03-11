// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_CLIENT_H_
#define PSYCHE_PSYCHED_CLIENT_H_

#include <set>
#include <string>

#include <base/macros.h>
#include <base/memory/scoped_ptr.h>

namespace protobinder {
class BinderProxy;
}  // namespace protobinder

namespace psyche {

class IPsycheClient;
class Service;

// A client that has requested one or more services from psyched.
class Client {
 public:
  explicit Client(scoped_ptr<protobinder::BinderProxy> client_proxy);
  ~Client();

  using ServiceSet = std::set<Service*>;
  const ServiceSet& services() const { return services_; }

  // Adds or removes a service that this client has requested to |services_|.
  // Ownership of |client| remains with the caller.
  void AddService(Service* service);
  void RemoveService(Service* service);

  // Handle notification that a service's state has changed.
  void HandleServiceStateChange(Service* service);

 private:
  // Passes |service|'s handle to the client.
  void SendServiceHandle(Service* service);

  scoped_ptr<protobinder::BinderProxy> proxy_;
  scoped_ptr<IPsycheClient> interface_;

  // Services that this client is holding connections to.
  ServiceSet services_;

  DISALLOW_COPY_AND_ASSIGN(Client);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_CLIENT_H_
