// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/client_stub.h"

#include <utility>

#include <protobinder/binder_proxy.h>

using protobinder::BinderProxy;

namespace psyche {

ClientStub::ClientStub(std::unique_ptr<BinderProxy> client_proxy)
    : client_proxy_(std::move(client_proxy)) {}

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

}  // namespace psyche
