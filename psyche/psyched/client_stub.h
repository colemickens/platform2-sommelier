// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_CLIENT_STUB_H_
#define PSYCHE_PSYCHED_CLIENT_STUB_H_

#include <memory>
#include <vector>

#include "psyche/psyched/client.h"

namespace protobinder {
class BinderProxy;
}  // protobinder

namespace psyche {

// A stub implementation of ClientInterface used for testing.
class ClientStub : public ClientInterface {
 public:
  explicit ClientStub(std::unique_ptr<protobinder::BinderProxy> client_proxy);
  ~ClientStub() override;

  using ServiceVector = std::vector<ServiceInterface*>;

  const ServiceVector& services_with_changed_states() const {
    return services_with_changed_states_;
  }
  void clear_services_with_changed_states() {
    services_with_changed_states_.clear();
  }

  // ClientInterface:
  const ServiceSet& GetServices() const override;
  void AddService(ServiceInterface* service) override;
  void RemoveService(ServiceInterface* service) override;
  void HandleServiceStateChange(ServiceInterface* service) override;

 private:
  std::unique_ptr<protobinder::BinderProxy> client_proxy_;
  ServiceSet services_;

  // Services that have called HandleServiceStateChange().
  ServiceVector services_with_changed_states_;

  DISALLOW_COPY_AND_ASSIGN(ClientStub);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_CLIENT_STUB_H_
