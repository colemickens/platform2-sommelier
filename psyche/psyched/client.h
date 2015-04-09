// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_CLIENT_H_
#define PSYCHE_PSYCHED_CLIENT_H_

#include <memory>
#include <set>
#include <string>

#include <base/macros.h>

#include "psyche/psyched/service_observer.h"

namespace protobinder {
class BinderProxy;
}  // namespace protobinder

namespace psyche {

class IPsycheClient;
class ServiceInterface;

// A client that has requested one or more services from psyched.
class ClientInterface {
 public:
  virtual ~ClientInterface() = default;

  using ServiceSet = std::set<ServiceInterface*>;
  virtual const ServiceSet& GetServices() const = 0;

  // Notifies the client that its request for |service_name| failed.
  virtual void ReportServiceRequestFailure(const std::string& service_name) = 0;

  // Adds or removes a service that this client has requested to |services_|.
  // Ownership of |service| remains with the caller.
  virtual void AddService(ServiceInterface* service) = 0;
  virtual void RemoveService(ServiceInterface* service) = 0;
};

// The real implementation of ClientInterface.
class Client : public ClientInterface, public ServiceObserver {
 public:
  explicit Client(std::unique_ptr<protobinder::BinderProxy> client_proxy);
  ~Client() override;

  // ClientInterface:
  const ServiceSet& GetServices() const override;
  void ReportServiceRequestFailure(const std::string& service_name) override;
  void AddService(ServiceInterface* service) override;
  void RemoveService(ServiceInterface* service) override;

  // ServiceObserver:
  void OnServiceProxyChange(ServiceInterface* service) override;

 private:
  // Passes |service_proxy| to the client for |service_name|. |service_proxy|
  // may be null to indicate a failed request.
  void SendServiceProxy(const std::string& service_name,
                        const protobinder::BinderProxy* service_proxy);

  std::unique_ptr<protobinder::BinderProxy> proxy_;
  std::unique_ptr<IPsycheClient> interface_;

  // Services that this client is holding connections to.
  ServiceSet services_;

  DISALLOW_COPY_AND_ASSIGN(Client);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_CLIENT_H_
