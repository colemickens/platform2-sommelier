// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/germ_connection.h"

#include <base/logging.h>
#include <germ/constants.h>
#include <protobinder/binder_proxy.h>
#include <protobinder/iinterface.h>

#include "psyche/proto_bindings/germ.pb.h"
#include "psyche/proto_bindings/germ.pb.rpc.h"

using protobinder::BinderProxy;
using protobinder::BinderToInterface;

using germ::IGerm;

namespace psyche {

// static
const char* GermConnection::ResultToString(Result result) {
  switch (result) {
    case Result::SUCCESS:
      return "SUCCESS";
    case Result::NO_CONNECTION:
      return "NO_CONNECTION";
    case Result::RPC_ERROR:
      return "RPC_ERROR";
    case Result::FAILED_REQUEST:
      return "FAILED_REQUEST";
  }
  NOTREACHED() << "Invalid result " << static_cast<int>(result);
  return "INVALID";
}

GermConnection::GermConnection() : service_(germ::kGermServiceName) {
  service_.AddObserver(this);
}

GermConnection::~GermConnection() {
  service_.RemoveObserver(this);
}

void GermConnection::SetProxy(std::unique_ptr<protobinder::BinderProxy> proxy) {
  // TODO(mcolagrosso): Verify that the transaction is coming from the proper
  // UID and report failure if not. See http://brbug.com/787.
  service_.SetProxy(std::move(proxy));
}

GermConnection::Result GermConnection::Launch(const soma::ContainerSpec& spec,
                                              int* pid) {
  CHECK(pid);
  if (!interface_)
    return Result::NO_CONNECTION;

  germ::LaunchRequest request;
  germ::LaunchResponse response;
  request.set_name(spec.name());
  request.mutable_spec()->CopyFrom(spec);

  int result = interface_->Launch(&request, &response);
  if (result != 0) {
    LOG(ERROR) << "Failed to launch container spec \"" << spec.name()
               << "\". RPC to germd returned " << result;
    return Result::RPC_ERROR;
  }

  if (!response.success()) {
    LOG(ERROR) << "Germ didn't return success when launching container spec \""
               << spec.name() << "\"";
    return Result::FAILED_REQUEST;
  }

  LOG(INFO) << "Launched container spec \"" << spec.name()
            << "\". pid: " << response.pid();

  *pid = response.pid();

  return Result::SUCCESS;
}

GermConnection::Result GermConnection::Terminate(int pid) {
  if (!interface_)
    return Result::NO_CONNECTION;

  germ::TerminateRequest request;
  germ::TerminateResponse response;
  request.set_pid(pid);

  int result = interface_->Terminate(&request, &response);
  if (result != 0) {
    LOG(ERROR) << "Failed to terminate container with init PID " << pid
               << ". RPC to germd returned " << result;
    return Result::RPC_ERROR;
  }

  if (!response.success()) {
    LOG(ERROR) << "Germ didn't return success when terminating container with "
                  "init PID " << pid;
    return Result::FAILED_REQUEST;
  }

  LOG(INFO) << "Terminated container with init PID " << pid;

  return Result::SUCCESS;
}

void GermConnection::OnServiceProxyChange(ServiceInterface* service) {
  DCHECK_EQ(service, &service_);
  if (service->GetProxy()) {
    LOG(INFO) << "Got connection to " << germ::kGermServiceName;
    interface_.reset(BinderToInterface<IGerm>(service->GetProxy()));
  } else {
    LOG(WARNING) << "Lost connection to " << germ::kGermServiceName;
    interface_.reset();
  }
}

}  // namespace psyche
