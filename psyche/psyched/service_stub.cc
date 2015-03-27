// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/service_stub.h"

#include <utility>

#include <protobinder/binder_proxy.h>

using protobinder::BinderProxy;

namespace psyche {

ServiceStub::ServiceStub(const std::string& name)
    : name_(name),
      state_(STATE_STOPPED) {
}

ServiceStub::~ServiceStub() = default;

const std::string& ServiceStub::GetName() const { return name_; }

ServiceInterface::State ServiceStub::GetState() const { return state_; }

protobinder::BinderProxy* ServiceStub::GetProxy() const { return proxy_.get(); }

void ServiceStub::SetProxy(std::unique_ptr<BinderProxy> proxy) {
  proxy_ = std::move(proxy);
  state_ = proxy_ ? STATE_STARTED : STATE_STOPPED;
}

void ServiceStub::AddClient(ClientInterface* client) {}

void ServiceStub::RemoveClient(ClientInterface* client) {}

bool ServiceStub::HasClient(ClientInterface* client) const { return false; }

}  // namespace psyche
