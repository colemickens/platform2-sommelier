// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/client_stub.h"

#include <protobinder/binder_proxy.h>

using protobinder::BinderProxy;

namespace psyche {

ClientStub::ClientStub(scoped_ptr<BinderProxy> client_proxy)
    : client_proxy_(client_proxy.Pass()) {}

ClientStub::~ClientStub() = default;

const ClientInterface::ServiceSet& ClientStub::GetServices() const {
  return services_;
}

void ClientStub::AddService(ServiceInterface* service) {
  services_.insert(service);
}

void ClientStub::RemoveService(ServiceInterface* service) {
  services_.erase(service);
}

void ClientStub::HandleServiceStateChange(ServiceInterface* service) {
  services_with_changed_states_.push_back(service);
}

}  // namespace psyche
