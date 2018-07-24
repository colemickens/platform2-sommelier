// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/cicerone/container_listener_impl.h"

#include <arpa/inet.h>
#include <inttypes.h>
#include <stdio.h>

#include <memory>
#include <string>

#include <base/bind.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/threading/thread_task_runner_handle.h>
#include <vm_applications/proto_bindings/apps.pb.h>
#include <vm_cicerone/proto_bindings/cicerone_service.pb.h>

#include "vm_tools/cicerone/service.h"

namespace {
constexpr char kIpv4Prefix[] = "ipv4:";
constexpr size_t kIpv4PrefixSize = sizeof(kIpv4Prefix) - 1;
// These rate limit settings ensure that the OpenUrl call can't be made more
// than 10 times in a 15 second interval approximately.
constexpr base::TimeDelta kOpenUrlRateWindow = base::TimeDelta::FromSeconds(15);
constexpr uint32_t kOpenUrlRateLimit = 10;

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

// Returns 0 on failure, otherwise the parsed vsock cid from a
// vsock:cid:port string.
uint32_t ExtractCidFromPeerAddress(const std::string& peer_address) {
  uint32_t cid = 0;
  sscanf(peer_address.c_str(), "vsock:%" SCNu32, &cid);
  return cid;
}

}  // namespace

namespace vm_tools {
namespace cicerone {

ContainerListenerImpl::ContainerListenerImpl(
    base::WeakPtr<vm_tools::cicerone::Service> service)
    : service_(service),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      open_url_count_(0),
      open_url_rate_window_start_(base::TimeTicks::Now()) {}

grpc::Status ContainerListenerImpl::ContainerReady(
    grpc::ServerContext* ctx,
    const vm_tools::container::ContainerStartupInfo* request,
    vm_tools::EmptyMessage* response) {
  std::string peer_address = ctx->peer();
  uint32_t ip = 0;
  uint32_t cid = ExtractCidFromPeerAddress(peer_address);
  if (cid == 0) {
    ip = ExtractIpFromPeerAddress(peer_address);
  }
  if (ip == 0 && cid == 0) {
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Failed parsing cid/IP for ContainerListener");
  }
  bool result = false;
  base::WaitableEvent event(false /*manual_reset*/,
                            false /*initially_signaled*/);
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&vm_tools::cicerone::Service::ContainerStartupCompleted,
                 service_, request->token(), cid, request->garcon_port(), ip,
                 &result, &event));
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
  uint32_t ip = 0;
  uint32_t cid = ExtractCidFromPeerAddress(peer_address);
  if (cid == 0) {
    ip = ExtractIpFromPeerAddress(peer_address);
  }
  if (ip == 0 && cid == 0) {
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Failed parsing cid/IP for ContainerListener");
  }
  bool result = false;
  base::WaitableEvent event(false /*manual_reset*/,
                            false /*initially_signaled*/);
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&vm_tools::cicerone::Service::ContainerShutdown, service_,
                 request->token(), cid, ip, &result, &event));
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
  uint32_t ip = 0;
  uint32_t cid = ExtractCidFromPeerAddress(peer_address);
  if (cid == 0) {
    ip = ExtractIpFromPeerAddress(peer_address);
  }
  if (ip == 0 && cid == 0) {
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Failed parsing cid/IP for ContainerListener");
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
      base::Bind(&vm_tools::cicerone::Service::UpdateApplicationList, service_,
                 request->token(), cid, ip, &app_list, &result, &event));
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
  // Check on rate limiting before we process this.
  base::TimeTicks now = base::TimeTicks::Now();
  if (now - open_url_rate_window_start_ > kOpenUrlRateWindow) {
    // Beyond the window, reset the window start time and counter.
    open_url_rate_window_start_ = now;
    open_url_count_ = 1;
  } else {
    open_url_count_++;
    if (open_url_count_ > kOpenUrlRateLimit) {
      // Only log the first one over the limit to prevent log spam if this is
      // getting hit quickly.
      LOG_IF(ERROR, open_url_count_ == kOpenUrlRateLimit + 1)
          << "OpenUrl rate limit hit, blocking requests until window closes";
      return grpc::Status(grpc::RESOURCE_EXHAUSTED,
                          "OpenUrl rate limit exceeded, blocking request");
    }
  }
  std::string peer_address = ctx->peer();
  uint32_t ip = 0;
  uint32_t cid = ExtractCidFromPeerAddress(peer_address);
  if (cid == 0) {
    ip = ExtractIpFromPeerAddress(peer_address);
  }
  if (ip == 0 && cid == 0) {
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Failed parsing cid/IP for ContainerListener");
  }
  base::WaitableEvent event(false /*manual_reset*/,
                            false /*initially_signaled*/);
  bool result = false;
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&vm_tools::cicerone::Service::OpenUrl, service_,
                 request->token(), request->url(), cid, ip, &result, &event));
  event.Wait();
  if (!result) {
    LOG(ERROR) << "Failure opening URL from ContainerListener";
    return grpc::Status(grpc::FAILED_PRECONDITION, "Failure in OpenUrl");
  }
  return grpc::Status::OK;
}

grpc::Status ContainerListenerImpl::InstallLinuxPackageProgress(
    grpc::ServerContext* ctx,
    const vm_tools::container::InstallLinuxPackageProgressInfo* request,
    vm_tools::EmptyMessage* response) {
  std::string peer_address = ctx->peer();
  uint32_t ip = 0;
  uint32_t cid = ExtractCidFromPeerAddress(peer_address);
  if (cid == 0) {
    ip = ExtractIpFromPeerAddress(peer_address);
  }
  if (ip == 0 && cid == 0) {
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Failed parsing cid/IP for ContainerListener");
  }
  InstallLinuxPackageProgressSignal progress_signal;
  if (!InstallLinuxPackageProgressSignal::Status_IsValid(
          static_cast<int>(request->status()))) {
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Invalid status field in protobuf request");
  }
  progress_signal.set_status(
      static_cast<InstallLinuxPackageProgressSignal::Status>(
          request->status()));
  progress_signal.set_progress_percent(request->progress_percent());
  progress_signal.set_failure_details(request->failure_details());
  base::WaitableEvent event(false /*manual_reset*/,
                            false /*initially_signaled*/);
  bool result = false;
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&vm_tools::cicerone::Service::InstallLinuxPackageProgress,
                 service_, request->token(), cid, ip, &progress_signal, &result,
                 &event));
  event.Wait();
  if (!result) {
    LOG(ERROR) << "Failure updating Linux package install progress from "
                  "ContainerListener";
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Failure in InstallLinuxPackageProgress");
  }

  return grpc::Status::OK;
}
}  // namespace cicerone
}  // namespace vm_tools
