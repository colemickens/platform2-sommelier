// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/soma_connection.h"

#include <utility>

#include <base/logging.h>
#include <protobinder/binder_proxy.h>

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
    case RESULT_SUCCESS:
      return "SUCCESS";
    case RESULT_RPC_ERROR:
      return "RPC_ERROR";
    case RESULT_UNKNOWN_SERVICE:
      return "UNKNOWN_SERVICE";
  }
  NOTREACHED() << "Invalid result " << result;
  return "INVALID";
}

SomaConnection::SomaConnection(std::unique_ptr<BinderProxy> proxy)
    : proxy_(std::move(proxy)),
      interface_(BinderToInterface<ISoma>(proxy_.get())) {
}

SomaConnection::~SomaConnection() = default;

SomaConnection::Result SomaConnection::GetContainerSpecForService(
    const std::string& service_name,
    ContainerSpec* spec_out) {
  DCHECK(spec_out);

  soma::GetContainerSpecRequest request;
  request.set_service_name(service_name);
  soma::GetContainerSpecResponse response;
  int result = interface_->GetContainerSpec(&request, &response);
  if (result != 0) {
    LOG(ERROR) << "RPC to somad returned " << result;
    return RESULT_RPC_ERROR;
  }

  if (!response.has_container_spec())
    return RESULT_UNKNOWN_SERVICE;

  *spec_out = response.container_spec();
  return RESULT_SUCCESS;
}

}  // namespace psyche
