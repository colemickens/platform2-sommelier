// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/container_listener_impl.h"

#include <arpa/inet.h>

#include <memory>
#include <string>

#include <base/bind.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/threading/thread_task_runner_handle.h>

#include "vm_tools/concierge/service.h"

namespace {
constexpr char kIpv4Prefix[] = "ipv4:";
constexpr size_t kIpv4PrefixSize = sizeof(kIpv4Prefix) - 1;
}  // namespace

namespace vm_tools {
namespace concierge {

ContainerListenerImpl::ContainerListenerImpl(
    base::WeakPtr<vm_tools::concierge::Service> service)
    : service_(service), task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

grpc::Status ContainerListenerImpl::ContainerReady(
    grpc::ServerContext* ctx,
    const vm_tools::container::ContainerStartupInfo* request,
    vm_tools::EmptyMessage* response) {
  // The peer address should be in the format "ipv4:aaa.bbb.ccc.ddd:eee".
  std::string peer_address = ctx->peer();
  if (!base::StartsWith(peer_address, kIpv4Prefix,
                        base::CompareCase::INSENSITIVE_ASCII)) {
    LOG(ERROR) << "Failed parsing non-IPv4 address: " << peer_address;
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Invalid peer non-IPv4 address for ContainerListener");
  }
  auto colon_pos = peer_address.find(':', kIpv4PrefixSize);
  if (colon_pos == std::string::npos) {
    LOG(ERROR) << "Invalid peer address, missing port: " << peer_address;
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Invalid peer for ContainerListener");
  }
  std::string peer_ip =
      peer_address.substr(kIpv4PrefixSize, colon_pos - kIpv4PrefixSize);
  uint32_t int_ip;
  if (inet_pton(AF_INET, peer_ip.c_str(), &int_ip) != 1) {
    LOG(ERROR) << "Failed parsing IPv4 address: " << peer_ip;
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Failed parsing IPv4 address for ContainerListener");
  }
  bool result = false;
  base::WaitableEvent event(false /*manual_reset*/,
                            false /*initially_signaled*/);
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&vm_tools::concierge::Service::ContainerStartupCompleted,
                 service_, request->token(), int_ip, &result, &event));
  event.Wait();
  if (!result) {
    LOG(ERROR) << "Received ContainerReady but could not find matching VM: "
               << peer_address;
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Cannot find VM for ContainerListener");
  }

  return grpc::Status::OK;
}

}  // namespace concierge
}  // namespace vm_tools
