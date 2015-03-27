// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_CLIENT_H_
#define PSYCHE_PSYCHED_CLIENT_H_

#include <memory>
#include <set>
#include <string>

#include <base/macros.h>

namespace protobinder {
class BinderProxy;
}  // namespace protobinder

namespace psyche {

class IPsycheClient;
class ServiceInterface;

// A client that has requested one or more services from psyched.
class ClientInterface {
 public:
  using ServiceSet = std::set<ServiceInterface*>;
  virtual const ServiceSet& GetServices() const = 0;

  virtual ~ClientInterface() = default;

  // Adds or removes a service that this client has requested to |services_|.
  // Ownership of |service| remains with the caller.
  virtual void AddService(ServiceInterface* service) = 0;
  virtual void RemoveService(ServiceInterface* service) = 0;

  // Handle notification that a service's state has changed.
  virtual void HandleServiceStateChange(ServiceInterface* service) = 0;
};

// The real implementation of ClientInterface.
class Client : public ClientInterface {
 public:
  explicit Client(std::unique_ptr<protobinder::BinderProxy> client_proxy);
  ~Client() override;

  // ClientInterface:
  const ServiceSet& GetServices() const override;
  void AddService(ServiceInterface* service) override;
  void RemoveService(ServiceInterface* service) override;
  void HandleServiceStateChange(ServiceInterface* service) override;

 private:
  // Passes |service|'s handle to the client.
  void SendServiceHandle(ServiceInterface* service);

  std::unique_ptr<protobinder::BinderProxy> proxy_;
  std::unique_ptr<IPsycheClient> interface_;

  // Services that this client is holding connections to.
  ServiceSet services_;

  DISALLOW_COPY_AND_ASSIGN(Client);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_CLIENT_H_
