// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/cicerone/tremplin_listener_impl.h"

#include <arpa/inet.h>
#include <inttypes.h>
#include <stdio.h>

#include <memory>
#include <string>

#include <base/bind.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/threading/thread_task_runner_handle.h>

#include "vm_tools/cicerone/service.h"

namespace vm_tools {
namespace cicerone {

namespace {

// Returns 0 on failure, otherwise returns the 32-bit vsock cid.
uint32_t ExtractCidFromPeerAddress(const std::string& peer_address) {
  uint32_t cid = 0;
  if (sscanf(peer_address.c_str(), "vsock:%" SCNu32, &cid) != 1) {
    LOG(WARNING) << "Failed to parse peer address " << peer_address;
    return 0;
  }

  return cid;
}

}  // namespace

TremplinListenerImpl::TremplinListenerImpl(
    base::WeakPtr<vm_tools::cicerone::Service> service)
    : service_(service), task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

grpc::Status TremplinListenerImpl::TremplinReady(
    grpc::ServerContext* ctx,
    const vm_tools::tremplin::TremplinStartupInfo* request,
    vm_tools::tremplin::EmptyMessage* response) {
  std::string peer_address = ctx->peer();
  uint32_t cid = ExtractCidFromPeerAddress(peer_address);
  if (cid == 0) {
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Failed parsing vsock cid for TremplinListener");
  }

  bool result = false;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&vm_tools::cicerone::Service::ConnectTremplin,
                            service_, cid, &result, &event));
  event.Wait();
  if (!result) {
    LOG(ERROR) << "Received TremplinReady but could not find matching VM: "
               << peer_address;
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Cannot find VM for TremplinListener");
  }

  return grpc::Status::OK;
}

grpc::Status TremplinListenerImpl::UpdateCreateStatus(
    grpc::ServerContext* ctx,
    const vm_tools::tremplin::ContainerCreationProgress* request,
    vm_tools::tremplin::EmptyMessage* response) {
  std::string peer_address = ctx->peer();
  uint32_t cid = ExtractCidFromPeerAddress(peer_address);
  if (cid == 0) {
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Failed parsing vsock cid for TremplinListener");
  }

  bool result = false;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  if (request->status() == tremplin::ContainerCreationProgress::DOWNLOADING) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&vm_tools::cicerone::Service::LxdContainerDownloading,
                   service_, cid, request->container_name(),
                   request->download_progress(), &result, &event));
  } else {
    vm_tools::cicerone::Service::CreateStatus status;
    switch (request->status()) {
      case tremplin::ContainerCreationProgress::CREATED:
        status = vm_tools::cicerone::Service::CreateStatus::CREATED;
        break;
      case tremplin::ContainerCreationProgress::DOWNLOAD_TIMED_OUT:
        status = vm_tools::cicerone::Service::CreateStatus::DOWNLOAD_TIMED_OUT;
        break;
      case tremplin::ContainerCreationProgress::CANCELLED:
        status = vm_tools::cicerone::Service::CreateStatus::CANCELLED;
        break;
      case tremplin::ContainerCreationProgress::FAILED:
        status = vm_tools::cicerone::Service::CreateStatus::FAILED;
        break;
      default:
        status = vm_tools::cicerone::Service::CreateStatus::UNKNOWN;
        break;
    }
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&vm_tools::cicerone::Service::LxdContainerCreated,
                              service_, cid, request->container_name(), status,
                              request->failure_reason(), &result, &event));
  }

  event.Wait();
  if (!result) {
    LOG(ERROR)
        << "Received UpdateCreateStatus RPC but could not find matching VM: "
        << peer_address;
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Cannot find VM for TremplinListener");
  }

  return grpc::Status::OK;
}

grpc::Status TremplinListenerImpl::UpdateStartStatus(
    grpc::ServerContext* ctx,
    const vm_tools::tremplin::ContainerStartProgress* request,
    vm_tools::tremplin::EmptyMessage* response) {
  std::string peer_address = ctx->peer();
  uint32_t cid = ExtractCidFromPeerAddress(peer_address);
  if (cid == 0) {
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Failed parsing vsock cid for TremplinListener");
  }

  bool result = false;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  vm_tools::cicerone::Service::StartStatus status;
  switch (request->status()) {
    case tremplin::ContainerStartProgress::STARTED:
      status = vm_tools::cicerone::Service::StartStatus::STARTED;
      break;
    case tremplin::ContainerStartProgress::CANCELLED:
      status = vm_tools::cicerone::Service::StartStatus::CANCELLED;
      break;
    case tremplin::ContainerStartProgress::FAILED:
      status = vm_tools::cicerone::Service::StartStatus::FAILED;
      break;
    default:
      status = vm_tools::cicerone::Service::StartStatus::UNKNOWN;
      break;
  }
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&vm_tools::cicerone::Service::LxdContainerStarting,
                            service_, cid, request->container_name(), status,
                            request->failure_reason(), &result, &event));

  event.Wait();
  if (!result) {
    LOG(ERROR)
        << "Received UpdateStartStatus RPC but could not find matching VM: "
        << peer_address;
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Cannot find VM for TremplinListener");
  }

  return grpc::Status::OK;
}

}  // namespace cicerone
}  // namespace vm_tools
