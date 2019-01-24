// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/cicerone/virtual_machine.h"

#include <arpa/inet.h>
#include <inttypes.h>

#include <algorithm>
#include <memory>
#include <utility>

#include <base/bind.h>
#include <base/guid.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/strings/stringprintf.h>
#include <google/protobuf/repeated_field.h>
#include <grpc++/grpc++.h>

#include "container_guest.grpc.pb.h"  // NOLINT(build/include)
#include "vm_tools/common/constants.h"

using std::string;

namespace vm_tools {
namespace cicerone {
namespace {

// How long to wait before timing out on regular RPCs.
constexpr int64_t kDefaultTimeoutSeconds = 60;

// How long to wait while doing more complex operations like starting or
// creating a container.
constexpr int64_t kLongOperationTimeoutSeconds = 120;

}  // namespace

VirtualMachine::VirtualMachine(uint32_t cid)
    : vsock_cid_(cid), weak_ptr_factory_(this) {}

VirtualMachine::~VirtualMachine() = default;

bool VirtualMachine::ConnectTremplin() {
  std::string tremplin_address =
      base::StringPrintf("vsock:%u:%u", vsock_cid_, kTremplinPort);
  if (!tremplin_testing_address_.empty()) {
    tremplin_address = tremplin_testing_address_;
  }
  tremplin_stub_ =
      std::make_unique<vm_tools::tremplin::Tremplin::Stub>(grpc::CreateChannel(
          tremplin_address, grpc::InsecureChannelCredentials()));
  return tremplin_stub_ != nullptr;
}

void VirtualMachine::OverrideTremplinAddressForTesting(
    const std::string& tremplin_address) {
  CHECK(!tremplin_stub_)
      << "Calling OverrideTremplinAddressForTesting too late";
  tremplin_testing_address_ = tremplin_address;
}

bool VirtualMachine::SetTimezone(
    const std::string& timezone_name,
    const std::string& posix_tz_string,
    VirtualMachine::SetTimezoneResults* out_results,
    std::string* out_error) {
  DCHECK(out_results);
  DCHECK(out_error);
  if (!tremplin_stub_) {
    *out_error = "tremplin is not connected";
    return false;
  }
  LOG(INFO) << "Setting timezone to: " << timezone_name;

  vm_tools::tremplin::SetTimezoneRequest request;
  vm_tools::tremplin::SetTimezoneResponse response;

  request.set_timezone_name(timezone_name);
  request.set_posix_tz_string(posix_tz_string);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status = tremplin_stub_->SetTimezone(&ctx, request, &response);
  if (!status.ok()) {
    LOG(ERROR) << "SetTimezone RPC failed: " << status.error_message();
    out_error->assign(status.error_message());
    return false;
  }

  int failure_count = response.failure_reasons_size();
  if (failure_count != 0) {
    LOG(ERROR) << "Failed to set timezone for " << failure_count
               << " containers";
  }
  out_results->failure_reasons.clear();
  for (int i = 0; i < failure_count; i++) {
    out_results->failure_reasons.push_back(response.failure_reasons(i));
  }

  out_results->successes = response.successes();
  return true;
}

bool VirtualMachine::RegisterContainer(const std::string& container_token,
                                       const uint32_t garcon_vsock_port,
                                       const std::string& container_ip) {
  // The token will be in the pending map if this is the first start of the
  // container. It will be in the main map if this is from a crash/restart of
  // the garcon process in the container.
  auto pending_iter = pending_containers_.find(container_token);
  if (pending_iter != pending_containers_.end()) {
    containers_[pending_iter->first] = std::move(pending_iter->second);
    pending_containers_.erase(pending_iter);
  } else {
    string container_name = GetContainerNameForToken(container_token);
    if (container_name.empty()) {
      return false;
    }
  }

  auto iter = containers_.find(container_token);
  std::string garcon_addr;
  if (garcon_vsock_port != 0) {
    garcon_addr = base::StringPrintf("vsock:%" PRIu32 ":%" PRIu32, vsock_cid_,
                                     garcon_vsock_port);
  } else {
    garcon_addr = base::StringPrintf("%s:%d", container_ip.c_str(),
                                     vm_tools::kGarconPort);
  }

  iter->second->ConnectToGarcon(garcon_addr);

  return true;
}

bool VirtualMachine::UnregisterContainer(const std::string& container_token) {
  auto iter = containers_.find(container_token);
  if (iter == containers_.end()) {
    return false;
  }
  containers_.erase(iter);
  return true;
}

std::string VirtualMachine::GenerateContainerToken(
    const std::string& container_name) {
  std::string token = base::GenerateGUID();
  pending_containers_[token] = std::make_unique<Container>(
      container_name, token, weak_ptr_factory_.GetWeakPtr());
  return token;
}

void VirtualMachine::CreateContainerWithTokenForTesting(
    const std::string& container_name, const std::string& container_token) {
  pending_containers_[container_token] = std::make_unique<Container>(
      container_name, container_token, weak_ptr_factory_.GetWeakPtr());
}

std::string VirtualMachine::GetContainerNameForToken(
    const std::string& container_token) {
  auto iter = containers_.find(container_token);
  if (iter == containers_.end()) {
    return "";
  }
  return iter->second->name();
}

Container* VirtualMachine::GetContainerForToken(
    const std::string& container_token) {
  auto iter = containers_.find(container_token);
  if (iter == containers_.end()) {
    return nullptr;
  }

  return iter->second.get();
}

Container* VirtualMachine::GetPendingContainerForToken(
    const std::string& container_token) {
  auto iter = pending_containers_.find(container_token);
  if (iter == pending_containers_.end()) {
    return nullptr;
  }

  return iter->second.get();
}

Container* VirtualMachine::GetContainerForName(
    const std::string& container_name) {
  for (auto iter = containers_.begin(); iter != containers_.end(); ++iter) {
    if (iter->second->name() == container_name) {
      return iter->second.get();
    }
  }
  return nullptr;
}

std::vector<std::string> VirtualMachine::GetContainerNames() {
  std::vector<std::string> retval;
  for (auto& container_entry : containers_) {
    retval.emplace_back(container_entry.second->name());
  }
  return retval;
}

VirtualMachine::CreateLxdContainerStatus VirtualMachine::CreateLxdContainer(
    const std::string& container_name,
    const std::string& image_server,
    const std::string& image_alias,
    std::string* out_error) {
  DCHECK(out_error);
  if (!tremplin_stub_) {
    *out_error = "tremplin is not connected";
    return VirtualMachine::CreateLxdContainerStatus::FAILED;
  }

  vm_tools::tremplin::CreateContainerRequest request;
  vm_tools::tremplin::CreateContainerResponse response;

  request.set_container_name(container_name);
  request.set_image_server(image_server);
  request.set_image_alias(image_alias);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kLongOperationTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status =
      tremplin_stub_->CreateContainer(&ctx, request, &response);
  if (!status.ok()) {
    LOG(ERROR) << "CreateContainer RPC failed: " << status.error_message();
    out_error->assign(status.error_message());
    return VirtualMachine::CreateLxdContainerStatus::FAILED;
  }

  if (response.status() != tremplin::CreateContainerResponse::CREATING &&
      response.status() != tremplin::CreateContainerResponse::EXISTS) {
    LOG(ERROR) << "Failed to create LXD container: "
               << response.failure_reason();
    out_error->assign(response.failure_reason());
    return VirtualMachine::CreateLxdContainerStatus::FAILED;
  }

  if (response.status() == tremplin::CreateContainerResponse::EXISTS) {
    return VirtualMachine::CreateLxdContainerStatus::EXISTS;
  }

  return VirtualMachine::CreateLxdContainerStatus::CREATING;
}

VirtualMachine::StartLxdContainerStatus VirtualMachine::StartLxdContainer(
    const std::string& container_name,
    const std::string& container_private_key,
    const std::string& host_public_key,
    const std::string& token,
    bool async,
    std::string* out_error) {
  DCHECK(out_error);
  if (!tremplin_stub_) {
    *out_error = "tremplin is not connected";
    return VirtualMachine::StartLxdContainerStatus::FAILED;
  }

  vm_tools::tremplin::StartContainerRequest request;
  vm_tools::tremplin::StartContainerResponse response;

  request.set_container_name(container_name);
  request.set_container_private_key(container_private_key);
  request.set_host_public_key(host_public_key);
  request.set_token(token);
  request.set_async(async);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kLongOperationTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status =
      tremplin_stub_->StartContainer(&ctx, request, &response);
  if (!status.ok()) {
    LOG(ERROR) << "StartContainer RPC failed: " << status.error_message();
    out_error->assign(status.error_message());
    return VirtualMachine::StartLxdContainerStatus::FAILED;
  }

  switch (response.status()) {
    case tremplin::StartContainerResponse::STARTING:
      return VirtualMachine::StartLxdContainerStatus::STARTING;
    case tremplin::StartContainerResponse::STARTED:
      return VirtualMachine::StartLxdContainerStatus::STARTED;
    case tremplin::StartContainerResponse::REMAPPING:
      return VirtualMachine::StartLxdContainerStatus::REMAPPING;
    case tremplin::StartContainerResponse::RUNNING:
      return VirtualMachine::StartLxdContainerStatus::RUNNING;
    case tremplin::StartContainerResponse::FAILED:
      LOG(ERROR) << "Failed to start LXD container: "
                 << response.failure_reason();
      out_error->assign(response.failure_reason());
      return VirtualMachine::StartLxdContainerStatus::FAILED;
    default:
      return VirtualMachine::StartLxdContainerStatus::UNKNOWN;
  }
}

VirtualMachine::GetLxdContainerUsernameStatus
VirtualMachine::GetLxdContainerUsername(const std::string& container_name,
                                        std::string* out_username,
                                        std::string* out_homedir,
                                        std::string* out_error) {
  DCHECK(out_username);
  DCHECK(out_homedir);
  DCHECK(out_error);
  if (!tremplin_stub_) {
    *out_error = "tremplin is not connected";
    return VirtualMachine::GetLxdContainerUsernameStatus::FAILED;
  }

  vm_tools::tremplin::GetContainerUsernameRequest request;
  vm_tools::tremplin::GetContainerUsernameResponse response;

  request.set_container_name(container_name);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status =
      tremplin_stub_->GetContainerUsername(&ctx, request, &response);
  if (!status.ok()) {
    LOG(ERROR) << "GetContainerUsername RPC failed: " << status.error_message();
    out_error->assign(status.error_message());
    return VirtualMachine::GetLxdContainerUsernameStatus::FAILED;
  }

  out_error->assign(response.failure_reason());
  out_username->assign(response.username());
  out_homedir->assign(response.homedir());

  switch (response.status()) {
    case tremplin::GetContainerUsernameResponse::UNKNOWN:
      return VirtualMachine::GetLxdContainerUsernameStatus::UNKNOWN;
    case tremplin::GetContainerUsernameResponse::SUCCESS:
      return VirtualMachine::GetLxdContainerUsernameStatus::SUCCESS;
    case tremplin::GetContainerUsernameResponse::CONTAINER_NOT_FOUND:
      return VirtualMachine::GetLxdContainerUsernameStatus::CONTAINER_NOT_FOUND;
    case tremplin::GetContainerUsernameResponse::CONTAINER_NOT_RUNNING:
      return VirtualMachine::GetLxdContainerUsernameStatus::
          CONTAINER_NOT_RUNNING;
    case tremplin::GetContainerUsernameResponse::USER_NOT_FOUND:
      return VirtualMachine::GetLxdContainerUsernameStatus::USER_NOT_FOUND;
    case tremplin::GetContainerUsernameResponse::FAILED:
      return VirtualMachine::GetLxdContainerUsernameStatus::FAILED;
    default:
      return VirtualMachine::GetLxdContainerUsernameStatus::UNKNOWN;
  }
}

VirtualMachine::SetUpLxdContainerUserStatus
VirtualMachine::SetUpLxdContainerUser(const std::string& container_name,
                                      const std::string& container_username,
                                      std::string* out_username,
                                      std::string* out_error) {
  DCHECK(out_username);
  DCHECK(out_error);
  if (!tremplin_stub_) {
    *out_error = "tremplin is not connected";
    return VirtualMachine::SetUpLxdContainerUserStatus::FAILED;
  }

  vm_tools::tremplin::SetUpUserRequest request;
  vm_tools::tremplin::SetUpUserResponse response;

  request.set_container_name(container_name);
  request.set_container_username(container_username);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status = tremplin_stub_->SetUpUser(&ctx, request, &response);
  out_username->assign(response.username());
  if (!status.ok()) {
    LOG(ERROR) << "SetUpUser RPC failed: " << status.error_message();
    out_error->assign(status.error_message());
    return VirtualMachine::SetUpLxdContainerUserStatus::FAILED;
  }

  if (response.status() != tremplin::SetUpUserResponse::EXISTS &&
      response.status() != tremplin::SetUpUserResponse::SUCCESS) {
    LOG(ERROR) << "Failed to set up user: " << response.failure_reason();
    out_error->assign(response.failure_reason());
    return VirtualMachine::SetUpLxdContainerUserStatus::FAILED;
  }

  if (response.status() == tremplin::SetUpUserResponse::EXISTS) {
    return VirtualMachine::SetUpLxdContainerUserStatus::EXISTS;
  }

  return VirtualMachine::SetUpLxdContainerUserStatus::SUCCESS;
}

VirtualMachine::GetLxdContainerInfoStatus VirtualMachine::GetLxdContainerInfo(
    const std::string& container_name,
    VirtualMachine::LxdContainerInfo* out_info,
    std::string* out_error) {
  DCHECK(out_info);
  DCHECK(out_error);
  if (!tremplin_stub_) {
    *out_error = "tremplin is not connected";
    return VirtualMachine::GetLxdContainerInfoStatus::FAILED;
  }

  vm_tools::tremplin::GetContainerInfoRequest request;
  vm_tools::tremplin::GetContainerInfoResponse response;

  request.set_container_name(container_name);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status =
      tremplin_stub_->GetContainerInfo(&ctx, request, &response);
  if (!status.ok()) {
    LOG(ERROR) << "GetContainerInfo RPC failed: " << status.error_message();
    out_error->assign(status.error_message());
    return VirtualMachine::GetLxdContainerInfoStatus::FAILED;
  }

  out_info->ipv4_address = response.ipv4_address();
  out_error->assign(response.failure_reason());

  switch (response.status()) {
    case tremplin::GetContainerInfoResponse::RUNNING:
      return VirtualMachine::GetLxdContainerInfoStatus::RUNNING;
    case tremplin::GetContainerInfoResponse::STOPPED:
      return VirtualMachine::GetLxdContainerInfoStatus::STOPPED;
    case tremplin::GetContainerInfoResponse::NOT_FOUND:
      return VirtualMachine::GetLxdContainerInfoStatus::NOT_FOUND;
    default:
      return VirtualMachine::GetLxdContainerInfoStatus::UNKNOWN;
  }
}

VirtualMachine::ExportLxdContainerStatus VirtualMachine::ExportLxdContainer(
    const std::string& container_name,
    const std::string& export_path,
    std::string* out_error) {
  DCHECK(out_error);
  if (!tremplin_stub_) {
    *out_error = "tremplin is not connected";
    return VirtualMachine::ExportLxdContainerStatus::FAILED;
  }

  vm_tools::tremplin::ExportContainerRequest request;
  vm_tools::tremplin::ExportContainerResponse response;

  request.set_container_name(container_name);
  request.set_export_path(export_path);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status =
      tremplin_stub_->ExportContainer(&ctx, request, &response);
  if (!status.ok()) {
    LOG(ERROR) << "ExportLxdContainer RPC failed: " << status.error_message();
    out_error->assign(status.error_message());
    return VirtualMachine::ExportLxdContainerStatus::FAILED;
  }

  switch (response.status()) {
    case tremplin::ExportContainerResponse::EXPORTING:
      return VirtualMachine::ExportLxdContainerStatus::EXPORTING;
    case tremplin::ExportContainerResponse::FAILED:
      return VirtualMachine::ExportLxdContainerStatus::FAILED;
    default:
      return VirtualMachine::ExportLxdContainerStatus::UNKNOWN;
  }
}

VirtualMachine::ImportLxdContainerStatus VirtualMachine::ImportLxdContainer(
    const std::string& container_name,
    const std::string& import_path,
    std::string* out_error) {
  DCHECK(out_error);
  if (!tremplin_stub_) {
    *out_error = "tremplin is not connected";
    return VirtualMachine::ImportLxdContainerStatus::FAILED;
  }

  vm_tools::tremplin::ImportContainerRequest request;
  vm_tools::tremplin::ImportContainerResponse response;

  request.set_container_name(container_name);
  request.set_import_path(import_path);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status =
      tremplin_stub_->ImportContainer(&ctx, request, &response);
  if (!status.ok()) {
    LOG(ERROR) << "ImportLxdContainer RPC failed: " << status.error_message();
    out_error->assign(status.error_message());
    return VirtualMachine::ImportLxdContainerStatus::FAILED;
  }

  switch (response.status()) {
    case tremplin::ImportContainerResponse::IMPORTING:
      return VirtualMachine::ImportLxdContainerStatus::IMPORTING;
    case tremplin::ImportContainerResponse::FAILED:
      return VirtualMachine::ImportLxdContainerStatus::FAILED;
    default:
      return VirtualMachine::ImportLxdContainerStatus::UNKNOWN;
  }
}
}  // namespace cicerone
}  // namespace vm_tools
