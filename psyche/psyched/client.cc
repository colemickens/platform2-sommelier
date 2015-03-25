// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/client.h"

#include <protobinder/binder_proxy.h>

#include "psyche/common/util.h"
#include "psyche/proto_bindings/psyche.pb.h"
#include "psyche/proto_bindings/psyche.pb.rpc.h"
#include "psyche/psyched/service.h"

namespace psyche {

Client::Client(scoped_ptr<protobinder::BinderProxy> client_proxy)
    : proxy_(client_proxy.Pass()),
      interface_(BinderToInterface<IPsycheClient>(proxy_.get())) {
}

Client::~Client() = default;

void Client::AddService(Service* service) {
  DCHECK(services_.find(service) == services_.end())
      << "Service \"" << service->name() << "\" already registered for client "
      << " with handle " << proxy_->handle();
  services_.insert(service);
  if (service->state() == Service::STATE_STARTED)
    SendServiceHandle(service);
}

void Client::RemoveService(Service* service) {
  services_.erase(service);
}

void Client::HandleServiceStateChange(Service* service) {
  DCHECK(services_.count(service))
      << "Service \"" << service->name() << "\" not registered for client with "
      << "handle " << proxy_->handle();
  if (service->state() == Service::STATE_STARTED)
    SendServiceHandle(service);
}

void Client::SendServiceHandle(Service* service) {
  ReceiveServiceRequest request;
  request.set_name(service->name());
  util::CopyBinderToProto(*(service->proxy()), request.mutable_binder());
  ReceiveServiceResponse response;
  int result = interface_->ReceiveService(&request, &response);
  if (result != 0) {
    LOG(WARNING) << "Failed to pass service \"" << service->name()
                 << "\" to client with handle " << proxy_->handle();
  }
}

}  // namespace psyche
