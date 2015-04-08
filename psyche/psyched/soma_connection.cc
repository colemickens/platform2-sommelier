// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/soma_connection.h"

#include <utility>

#include <base/logging.h>
#include <protobinder/binder_proxy.h>
#include <soma/constants.h>

#include "psyche/proto_bindings/soma.pb.h"
#include "psyche/proto_bindings/soma.pb.rpc.h"
#include "psyche/proto_bindings/soma_container_spec.pb.h"

using protobinder::BinderProxy;
using protobinder::BinderToInterface;

using soma::ContainerSpec;
using soma::ISoma;

namespace psyche {

// static
const char* SomaConnection::ResultToString(Result result) {
  switch (result) {
    case Result::SUCCESS:
      return "SUCCESS";
    case Result::NO_SOMA_CONNECTION:
      return "NO_SOMA_CONNECTION";
    case Result::RPC_ERROR:
      return "RPC_ERROR";
    case Result::UNKNOWN_SERVICE:
      return "UNKNOWN_SERVICE";
  }
  NOTREACHED() << "Invalid result " << static_cast<int>(result);
  return "INVALID";
}

SomaConnection::SomaConnection() : service_(soma::kSomaServiceName) {
  service_.AddObserver(this);
}

SomaConnection::~SomaConnection() {
  service_.RemoveObserver(this);
}

void SomaConnection::SetProxy(std::unique_ptr<protobinder::BinderProxy> proxy) {
  // TODO(derat): Verify that the transaction is coming from the proper UID and
  // report failure if not.
  service_.SetProxy(std::move(proxy));
}

SomaConnection::Result SomaConnection::GetContainerSpecForService(
    const std::string& service_name,
    ContainerSpec* spec_out) {
  DCHECK(spec_out);

  if (!interface_)
    return Result::NO_SOMA_CONNECTION;

  soma::GetContainerSpecRequest request;
  request.set_service_name(service_name);
  soma::GetContainerSpecResponse response;
  int result = interface_->GetContainerSpec(&request, &response);
  if (result != 0) {
    LOG(ERROR) << "RPC to somad returned " << result;
    return Result::RPC_ERROR;
  }

  if (!response.has_container_spec())
    return Result::UNKNOWN_SERVICE;

  *spec_out = response.container_spec();
  return Result::SUCCESS;
}

void SomaConnection::OnServiceProxyChange(ServiceInterface* service) {
  DCHECK_EQ(service, &service_);
  if (service->GetProxy()) {
    LOG(INFO) << "Got connection to somad";
    interface_.reset(BinderToInterface<ISoma>(service->GetProxy()));
  } else {
    LOG(WARNING) << "Lost connection to somad";
    interface_.reset();
  }
}

}  // namespace psyche
