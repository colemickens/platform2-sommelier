// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/service_stub.h"

#include <utility>

#include <protobinder/binder_proxy.h>

using protobinder::BinderProxy;

namespace psyche {

ServiceStub::ServiceStub(const std::string& name) : name_(name) {}

ServiceStub::~ServiceStub() = default;

void ServiceStub::SetProxyForTesting(std::unique_ptr<BinderProxy> proxy) {
  proxy_ = std::move(proxy);
}

const std::string& ServiceStub::GetName() const { return name_; }

protobinder::BinderProxy* ServiceStub::GetProxy() const { return proxy_.get(); }

void ServiceStub::SetProxy(std::unique_ptr<BinderProxy> proxy) {
  CHECK(proxy);
  proxy_ = std::move(proxy);
}

void ServiceStub::AddClient(ClientInterface* client) {
  CHECK(client);
  CHECK(clients_.insert(client).second)
      << "Client " << client << " already added to \"" << name_ << "\"";
}

void ServiceStub::RemoveClient(ClientInterface* client) {
  CHECK(client);
  CHECK_EQ(clients_.erase(client), 1U)
      << "Client " << client << " not present in \"" << name_ << "\"";
}

bool ServiceStub::HasClient(ClientInterface* client) const {
  CHECK(client);
  return clients_.count(client);
}

void ServiceStub::AddObserver(ServiceObserver* observer) {}

void ServiceStub::RemoveObserver(ServiceObserver* observer) {}

}  // namespace psyche
