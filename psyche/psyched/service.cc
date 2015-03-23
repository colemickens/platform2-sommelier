// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/service.h"

#include <vector>

#include <base/bind.h>
#include <protobinder/binder_proxy.h>

#include "psyche/proto_bindings/psyche.pb.h"
#include "psyche/proto_bindings/psyche.pb.rpc.h"
#include "psyche/psyched/client.h"

using protobinder::BinderProxy;
using protobinder::BinderToInterface;

namespace psyche {

Service::Service(const std::string& name)
    : name_(name),
      state_(STATE_STOPPED),
      weak_ptr_factory_(this) {
}

Service::~Service() = default;

const std::string& Service::GetName() const { return name_; }

ServiceInterface::State Service::GetState() const { return state_; }

protobinder::BinderProxy* Service::GetProxy() const { return proxy_.get(); }

void Service::SetProxy(scoped_ptr<BinderProxy> proxy) {
  proxy_ = proxy.Pass();
  proxy_->SetDeathCallback(base::Bind(&Service::HandleBinderDeath,
                                      weak_ptr_factory_.GetWeakPtr()));
  state_ = STATE_STARTED;
  NotifyClientsAboutStateChange();
}

void Service::AddClient(ClientInterface* client) {
  clients_.insert(client);
}

void Service::RemoveClient(ClientInterface* client) {
  clients_.erase(client);
}

bool Service::HasClient(ClientInterface* client) const {
  return clients_.count(client);
}

void Service::NotifyClientsAboutStateChange() {
  for (auto client : clients_)
    client->HandleServiceStateChange(this);
}

void Service::HandleBinderDeath() {
  LOG(INFO) << "Got binder death notification for \"" << name_ << "\"";

  // TODO(derat): Automatically restart the service.
  state_ = STATE_STOPPED;
  NotifyClientsAboutStateChange();
}

}  // namespace psyche
