// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/client.h"

#include <utility>

#include <protobinder/binder_proxy.h>

#include "psyche/common/util.h"
#include "psyche/proto_bindings/psyche.pb.h"
#include "psyche/proto_bindings/psyche.pb.rpc.h"
#include "psyche/psyched/service.h"

namespace psyche {

Client::Client(std::unique_ptr<protobinder::BinderProxy> client_proxy)
    : proxy_(std::move(client_proxy)),
      interface_(BinderToInterface<IPsycheClient>(proxy_.get())) {
}

Client::~Client() = default;

const ClientInterface::ServiceSet& Client::GetServices() const {
  return services_;
}

void Client::AddService(ServiceInterface* service) {
  DCHECK(services_.find(service) == services_.end())
      << "Service \"" << service->GetName() << "\" already registered for "
      << "client with handle " << proxy_->handle();
  services_.insert(service);
  if (service->GetState() == ServiceInterface::STATE_STARTED)
    SendServiceHandle(service);
}

void Client::RemoveService(ServiceInterface* service) {
  services_.erase(service);
}

void Client::HandleServiceStateChange(ServiceInterface* service) {
  DCHECK(services_.count(service))
      << "Service \"" << service->GetName() << "\" not registered for client "
      << "with handle " << proxy_->handle();
  if (service->GetState() == ServiceInterface::STATE_STARTED)
    SendServiceHandle(service);
}

void Client::SendServiceHandle(ServiceInterface* service) {
  ReceiveServiceRequest request;
  request.set_name(service->GetName());
  util::CopyBinderToProto(*(service->GetProxy()), request.mutable_binder());
  ReceiveServiceResponse response;
  int result = interface_->ReceiveService(&request, &response);
  if (result != 0) {
    LOG(WARNING) << "Failed to pass service \"" << service->GetName()
                 << "\" to client with handle " << proxy_->handle();
  }
}

}  // namespace psyche
