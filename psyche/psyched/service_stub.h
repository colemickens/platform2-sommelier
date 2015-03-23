// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_SERVICE_STUB_H_
#define PSYCHE_PSYCHED_SERVICE_STUB_H_

#include <string>

#include <base/memory/scoped_ptr.h>

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

  void set_state(State state) { state_ = state; }

  // ServiceInterface:
  const std::string& GetName() const override;
  State GetState() const override;
  protobinder::BinderProxy* GetProxy() const override;
  void SetProxy(scoped_ptr<protobinder::BinderProxy> proxy) override;
  void AddClient(ClientInterface* client) override;
  void RemoveClient(ClientInterface* client) override;
  bool HasClient(ClientInterface* client) const override;

 private:
  // The name of the service.
  std::string name_;

  // The service's current state.
  State state_;

  // The connection to the service that will be passed to clients.
  scoped_ptr<protobinder::BinderProxy> proxy_;

  DISALLOW_COPY_AND_ASSIGN(ServiceStub);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_SERVICE_STUB_H_
