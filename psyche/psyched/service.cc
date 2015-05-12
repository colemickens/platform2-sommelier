// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/service.h"

#include <utility>
#include <vector>

#include <base/bind.h>
#include <protobinder/binder_proxy.h>

#include "psyche/proto_bindings/psyche.pb.h"
#include "psyche/proto_bindings/psyche.pb.rpc.h"
#include "psyche/psyched/client.h"
#include "psyche/psyched/service_observer.h"

using protobinder::BinderProxy;

namespace psyche {

Service::Service(const std::string& name)
    : name_(name),
      timeout_pending_(true),
      weak_ptr_factory_(this) {
}

Service::~Service() = default;

const std::string& Service::GetName() const { return name_; }

BinderProxy* Service::GetProxy() const { return proxy_.get(); }

void Service::SetProxy(std::unique_ptr<BinderProxy> proxy) {
  DCHECK(proxy);
  timeout_pending_ = false;
  proxy_ = std::move(proxy);
  proxy_->SetDeathCallback(base::Bind(&Service::HandleBinderDeath,
                                      weak_ptr_factory_.GetWeakPtr()));
  FOR_EACH_OBSERVER(ServiceObserver, observers_, OnServiceProxyChange(this));
}

void Service::AddClient(ClientInterface* client) {
  clients_.insert(client);
  // If we don't have a proxy, and one isn't pending, notify the client of
  // failure immediately.
  if (!proxy_.get() && !timeout_pending_)
    client->ReportServiceRequestFailure(name_);
}

void Service::RemoveClient(ClientInterface* client) {
  clients_.erase(client);
}

bool Service::HasClient(ClientInterface* client) const {
  return clients_.count(client);
}

void Service::AddObserver(ServiceObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void Service::RemoveObserver(ServiceObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void Service::OnCellLaunched() {
  timeout_pending_ = true;
}

void Service::OnServiceUnavailable() {
  timeout_pending_ = false;
  for (const auto& client_it : clients_) {
    client_it->ReportServiceRequestFailure(name_);
  }
}

void Service::HandleBinderDeath() {
  LOG(INFO) << "Got binder death notification for \"" << name_ << "\"";
  proxy_.reset();
  FOR_EACH_OBSERVER(ServiceObserver, observers_, OnServiceProxyChange(this));
}

}  // namespace psyche
