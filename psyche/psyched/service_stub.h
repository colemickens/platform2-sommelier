// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_SERVICE_STUB_H_
#define PSYCHE_PSYCHED_SERVICE_STUB_H_

#include <memory>
#include <set>
#include <string>

#include "psyche/psyched/service.h"

namespace protobinder {
class BinderProxy;
}  // namespace protobinder

namespace psyche {

// Stub implementation of ServiceInterface used for testing.
class ServiceStub : public ServiceInterface {
 public:
  explicit ServiceStub(const std::string& name);
  ~ServiceStub() override;

  size_t num_clients() const { return clients_.size(); }

  // Similar to SetProxy() but allows its argument to be null. Used to simulate
  // the service clearing its proxy in response to a binder death notification.
  void SetProxyForTesting(std::unique_ptr<protobinder::BinderProxy> proxy);

  // ServiceInterface:
  const std::string& GetName() const override;
  protobinder::BinderProxy* GetProxy() const override;
  void SetProxy(std::unique_ptr<protobinder::BinderProxy> proxy) override;
  void AddClient(ClientInterface* client) override;
  void RemoveClient(ClientInterface* client) override;
  bool HasClient(ClientInterface* client) const override;
  void AddObserver(ServiceObserver* observer) override;
  void RemoveObserver(ServiceObserver* observer) override;

 private:
  // The name of the service.
  std::string name_;

  // The connection to the service that will be passed to clients.
  std::unique_ptr<protobinder::BinderProxy> proxy_;

  // Clients registered via AddClient().
  using ClientSet = std::set<ClientInterface*>;
  ClientSet clients_;

  DISALLOW_COPY_AND_ASSIGN(ServiceStub);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_SERVICE_STUB_H_
