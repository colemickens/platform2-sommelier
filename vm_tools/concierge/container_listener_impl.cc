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

namespace vm_tools {
namespace concierge {

ContainerListenerImpl::ContainerListenerImpl(
    base::WeakPtr<vm_tools::concierge::Service> service)
    : service_(service), task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

grpc::Status ContainerListenerImpl::ContainerReady(
    grpc::ServerContext* ctx,
    const vm_tools::container::ContainerStartupInfo* request,
    vm_tools::EmptyMessage* response) {
  std::string peer_address = ctx->peer();
  uint32_t ip = ExtractIpFromPeerAddress(peer_address);
  if (ip == 0) {
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Failed parsing IPv4 address for ContainerListener");
  }
  bool result = false;
  base::WaitableEvent event(false /*manual_reset*/,
                            false /*initially_signaled*/);
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&vm_tools::concierge::Service::ContainerStartupCompleted,
                 service_, request->token(), ip, &result, &event));
  event.Wait();
  if (!result) {
    LOG(ERROR) << "Received ContainerReady but could not find matching VM: "
               << peer_address;
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Cannot find VM for ContainerListener");
  }

  return grpc::Status::OK;
}

grpc::Status ContainerListenerImpl::ContainerShutdown(
    grpc::ServerContext* ctx,
    const vm_tools::container::ContainerShutdownInfo* request,
    vm_tools::EmptyMessage* response) {
  std::string peer_address = ctx->peer();
  uint32_t ip = ExtractIpFromPeerAddress(peer_address);
  if (ip == 0) {
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Failed parsing IPv4 address for ContainerListener");
  }
  bool result = false;
  base::WaitableEvent event(false /*manual_reset*/,
                            false /*initially_signaled*/);
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&vm_tools::concierge::Service::ContainerShutdown,
                            service_, request->token(), ip, &result, &event));
  event.Wait();
  if (!result) {
    LOG(ERROR) << "Received ContainerShutdown but could not find matching VM: "
               << peer_address;
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Cannot find VM for ContainerListener");
  }

  return grpc::Status::OK;
}

grpc::Status ContainerListenerImpl::UpdateApplicationList(
    grpc::ServerContext* ctx,
    const vm_tools::container::UpdateApplicationListRequest* request,
    vm_tools::EmptyMessage* response) {
  std::string peer_address = ctx->peer();
  uint32_t ip = ExtractIpFromPeerAddress(peer_address);
  if (ip == 0) {
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Failed parsing IPv4 address for ContainerListener");
  }
  vm_tools::apps::ApplicationList app_list;
  // vm_name and container_name are set in the UpdateApplicationList call but we
  // need to copy everything else out of the incoming protobuf here.
  for (const auto& app_in : request->application()) {
    auto app_out = app_list.add_apps();
    // Set the non-repeating fields first.
    app_out->set_desktop_file_id(app_in.desktop_file_id());
    app_out->set_no_display(app_in.no_display());
    app_out->set_startup_wm_class(app_in.startup_wm_class());
    app_out->set_startup_notify(app_in.startup_notify());
    // Set the mime types.
    for (const auto& mime_type : app_in.mime_types()) {
      app_out->add_mime_types(mime_type);
    }
    // Set the names & comments.
    if (app_in.has_name()) {
      auto name_out = app_out->mutable_name();
      for (const auto& names : app_in.name().values()) {
        auto curr_name = name_out->add_values();
        curr_name->set_locale(names.locale());
        curr_name->set_value(names.value());
      }
    }
    if (app_in.has_comment()) {
      auto comment_out = app_out->mutable_comment();
      for (const auto& comments : app_in.comment().values()) {
        auto curr_comment = comment_out->add_values();
        curr_comment->set_locale(comments.locale());
        curr_comment->set_value(comments.value());
      }
    }
  }
  base::WaitableEvent event(false /*manual_reset*/,
                            false /*initially_signaled*/);
  bool result = false;
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&vm_tools::concierge::Service::UpdateApplicationList, service_,
                 request->token(), ip, &app_list, &result, &event));
  event.Wait();
  if (!result) {
    LOG(ERROR) << "Failure updating application list from ContainerListener";
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Failure in UpdateApplicationList");
  }

  return grpc::Status::OK;
}

grpc::Status ContainerListenerImpl::OpenUrl(
    grpc::ServerContext* ctx,
    const vm_tools::container::OpenUrlRequest* request,
    vm_tools::EmptyMessage* response) {
  std::string peer_address = ctx->peer();
  uint32_t ip = ExtractIpFromPeerAddress(peer_address);
  if (ip == 0) {
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Failed parsing IPv4 address for ContainerListener");
  }
  base::WaitableEvent event(false /*manual_reset*/,
                            false /*initially_signaled*/);
  bool result = false;
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&vm_tools::concierge::Service::OpenUrl, service_,
                            request->url(), ip, &result, &event));
  event.Wait();
  if (!result) {
    LOG(ERROR) << "Failure opening URL from ContainerListener";
    return grpc::Status(grpc::FAILED_PRECONDITION, "Failure in OpenUrl");
  }
  return grpc::Status::OK;
}

}  // namespace concierge
}  // namespace vm_tools
