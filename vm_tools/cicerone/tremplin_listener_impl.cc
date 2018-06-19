// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/cicerone/tremplin_listener_impl.h"

#include <arpa/inet.h>

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
constexpr char kIpv4Prefix[] = "ipv4:";
constexpr size_t kIpv4PrefixSize = sizeof(kIpv4Prefix) - 1;

// Returns 0 on failure, otherwise the parsed 32 bit IP address from an
// ipv4:aaa.bbb.ccc.ddd:eee string.
uint32_t ExtractIpFromPeerAddress(const std::string& peer_address) {
  if (!base::StartsWith(peer_address, kIpv4Prefix,
                        base::CompareCase::INSENSITIVE_ASCII)) {
    LOG(ERROR) << "Failed parsing non-IPv4 address: " << peer_address;
    return 0;
  }
  auto colon_pos = peer_address.find(':', kIpv4PrefixSize);
  if (colon_pos == std::string::npos) {
    LOG(ERROR) << "Invalid peer address, missing port: " << peer_address;
    return 0;
  }
  uint32_t int_ip;
  std::string peer_ip =
      peer_address.substr(kIpv4PrefixSize, colon_pos - kIpv4PrefixSize);
  if (inet_pton(AF_INET, peer_ip.c_str(), &int_ip) != 1) {
    LOG(ERROR) << "Failed parsing IPv4 address: " << peer_ip;
    return 0;
  }
  return int_ip;
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
  uint32_t ipv4_address = ExtractIpFromPeerAddress(peer_address);
  if (ipv4_address == 0) {
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Failed parsing IPv4 address for TremplinListener");
  }

  bool result = false;
  base::WaitableEvent event(false /*manual_reset*/,
                            false /*initially_signaled*/);
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&vm_tools::cicerone::Service::ConnectTremplin,
                            service_, ipv4_address, &result, &event));
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
  uint32_t ip = ExtractIpFromPeerAddress(peer_address);
  if (ip == 0) {
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Failed parsing IPv4 address for TremplinListener");
  }

  bool result = false;
  base::WaitableEvent event(false /*manual_reset*/,
                            false /*initially_signaled*/);
  if (request->status() == tremplin::ContainerCreationProgress::DOWNLOADING) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&vm_tools::cicerone::Service::LxdContainerDownloading,
                   service_, ip, request->container_name(),
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
                              service_, ip, request->container_name(), status,
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

}  // namespace cicerone
}  // namespace vm_tools
