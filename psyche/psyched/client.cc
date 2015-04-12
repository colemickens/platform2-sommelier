// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/client.h"

#include <utility>

#include <protobinder/binder_proxy.h>
#include <protobinder/proto_util.h>

#include "psyche/proto_bindings/psyche.pb.h"
#include "psyche/proto_bindings/psyche.pb.rpc.h"
#include "psyche/psyched/service.h"

using protobinder::BinderProxy;
using protobinder::BinderToInterface;

namespace psyche {

Client::Client(std::unique_ptr<BinderProxy> client_proxy)
    : proxy_(std::move(client_proxy)),
      interface_(BinderToInterface<IPsycheClient>(proxy_.get())) {
}

Client::~Client() {
  while (!services_.empty())
    RemoveService(*(services_.begin()));
}

const ClientInterface::ServiceSet& Client::GetServices() const {
  return services_;
}

void Client::ReportServiceRequestFailure(const std::string& service_name) {
  SendServiceProxy(service_name, nullptr);
}

void Client::AddService(ServiceInterface* service) {
  DCHECK(services_.find(service) == services_.end())
      << "Service \"" << service->GetName() << "\" already registered for "
      << "client with handle " << proxy_->handle();
  service->AddObserver(this);
  services_.insert(service);
  if (service->GetProxy())
    SendServiceProxy(service->GetName(), service->GetProxy());
}

void Client::RemoveService(ServiceInterface* service) {
  service->RemoveObserver(this);
  services_.erase(service);
}

void Client::OnServiceProxyChange(ServiceInterface* service) {
  DCHECK(service);
  CHECK(services_.count(service)) << "Service \"" << service->GetName()
                                  << "\" not registered for client "
                                  << "with handle " << proxy_->handle();
  if (service->GetProxy())
    SendServiceProxy(service->GetName(), service->GetProxy());
}

void Client::SendServiceProxy(const std::string& service_name,
                              const protobinder::BinderProxy* service_proxy) {
  ReceiveServiceRequest request;
  request.set_name(service_name);
  if (service_proxy)
    protobinder::StoreBinderInProto(*service_proxy, request.mutable_binder());
  int result = interface_->ReceiveService(&request);
  if (result != 0) {
    LOG(WARNING) << "Failed to pass service \"" << service_name
                 << "\" to client with handle " << proxy_->handle()
                 << "; got " << result;
  }
}

}  // namespace psyche
