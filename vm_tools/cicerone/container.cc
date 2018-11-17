// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/cicerone/container.h"

#include <arpa/inet.h>

#include <algorithm>
#include <memory>
#include <utility>

#include <base/bind.h>
#include <base/guid.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/strings/stringprintf.h>
#include <google/protobuf/repeated_field.h>
#include <grpcpp/grpcpp.h>

#include "container_guest.grpc.pb.h"  // NOLINT(build/include)
#include "vm_tools/common/constants.h"

using std::string;

namespace vm_tools {
namespace cicerone {
namespace {

// How long to wait before timing out on regular RPCs.
constexpr int64_t kDefaultTimeoutSeconds = 60;

}  // namespace

Container::Container(const std::string& name,
                     const std::string& token,
                     base::WeakPtr<VirtualMachine> vm)
    : name_(name), token_(token), vm_(vm) {}

// Sets the container's IPv4 address.
void Container::set_ipv4_address(uint32_t ipv4_address) {
  ipv4_address_ = ipv4_address;
}

void Container::set_drivefs_mount_path(std::string drivefs_mount_path) {
  drivefs_mount_path_ = drivefs_mount_path;
}

void Container::set_homedir(const std::string& homedir) {
  homedir_ = homedir;
}

void Container::ConnectToGarcon(const std::string& addr) {
  garcon_stub_ = std::make_unique<vm_tools::container::Garcon::Stub>(
      grpc::CreateChannel(addr, grpc::InsecureChannelCredentials()));
}

bool Container::LaunchContainerApplication(
    const std::string& desktop_file_id,
    std::vector<std::string> files,
    vm_tools::container::LaunchApplicationRequest::DisplayScaling
        display_scaling,
    std::string* out_error) {
  CHECK(out_error);
  vm_tools::container::LaunchApplicationRequest container_request;
  vm_tools::container::LaunchApplicationResponse container_response;
  container_request.set_desktop_file_id(desktop_file_id);
  std::copy(std::make_move_iterator(files.begin()),
            std::make_move_iterator(files.end()),
            google::protobuf::RepeatedFieldBackInserter(
                container_request.mutable_files()));
  container_request.set_display_scaling(display_scaling);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status = garcon_stub_->LaunchApplication(&ctx, container_request,
                                                        &container_response);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to launch application " << desktop_file_id
               << " in container " << name_ << ": " << status.error_message();
    out_error->assign("gRPC failure launching application: " +
                      status.error_message());
    return false;
  }
  out_error->assign(container_response.failure_reason());
  return container_response.success();
}

bool Container::LaunchVshd(uint32_t port, std::string* out_error) {
  vm_tools::container::LaunchVshdRequest container_request;
  vm_tools::container::LaunchVshdResponse container_response;
  container_request.set_port(port);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status =
      garcon_stub_->LaunchVshd(&ctx, container_request, &container_response);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to launch vshd in container " << name_ << ": "
               << status.error_message() << " code: " << status.error_code();
    out_error->assign("gRPC failure launching vshd in container: " +
                      status.error_message());
    return false;
  }
  out_error->assign(container_response.failure_reason());
  return container_response.success();
}

bool Container::GetDebugInformation(std::string* out) {
  vm_tools::container::GetDebugInformationRequest container_request;
  vm_tools::container::GetDebugInformationResponse container_response;

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status = garcon_stub_->GetDebugInformation(
      &ctx, container_request, &container_response);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to get debug information in container " << name_
               << ": " << status.error_message()
               << " code: " << status.error_code();
    out->assign("gRPC failure to get debug information in container: " +
                status.error_message());
    return false;
  }
  out->assign(container_response.debug_information());
  return true;
}

bool Container::GetContainerAppIcon(std::vector<std::string> desktop_file_ids,
                                    uint32_t icon_size,
                                    uint32_t scale,
                                    std::vector<Icon>* icons) {
  CHECK(icons);

  vm_tools::container::IconRequest container_request;
  vm_tools::container::IconResponse container_response;

  std::copy(std::make_move_iterator(desktop_file_ids.begin()),
            std::make_move_iterator(desktop_file_ids.end()),
            google::protobuf::RepeatedFieldBackInserter(
                container_request.mutable_desktop_file_ids()));
  container_request.set_icon_size(icon_size);
  container_request.set_scale(scale);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status =
      garcon_stub_->GetIcon(&ctx, container_request, &container_response);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to get icons in container " << name_ << ": "
               << status.error_message();
    return false;
  }

  for (auto& icon : *container_response.mutable_desktop_icons()) {
    icons->emplace_back(
        Icon{.desktop_file_id = std::move(*icon.mutable_desktop_file_id()),
             .content = std::move(*icon.mutable_icon())});
  }
  return true;
}

bool Container::GetLinuxPackageInfo(const std::string& file_path,
                                    const std::string& package_name,
                                    LinuxPackageInfo* out_pkg_info,
                                    std::string* out_error) {
  CHECK(out_pkg_info);
  vm_tools::container::LinuxPackageInfoRequest container_request;
  vm_tools::container::LinuxPackageInfoResponse container_response;
  container_request.set_file_path(file_path);
  container_request.set_package_name(package_name);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status = garcon_stub_->GetLinuxPackageInfo(
      &ctx, container_request, &container_response);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to get Linux package info from container " << name_
               << ": " << status.error_message()
               << " code: " << status.error_code();
    out_error->assign(
        "gRPC failure getting Linux package info from container: " +
        status.error_message());
    return false;
  }
  out_error->assign(container_response.failure_reason());
  out_pkg_info->package_id = std::move(container_response.package_id());
  out_pkg_info->license = std::move(container_response.license());
  out_pkg_info->description = std::move(container_response.description());
  out_pkg_info->project_url = std::move(container_response.project_url());
  out_pkg_info->size = container_response.size();
  out_pkg_info->summary = std::move(container_response.summary());
  return container_response.success();
}

vm_tools::container::InstallLinuxPackageResponse::Status
Container::InstallLinuxPackage(const std::string& file_path,
                               const std::string& package_id,
                               std::string* out_error) {
  vm_tools::container::InstallLinuxPackageRequest container_request;
  vm_tools::container::InstallLinuxPackageResponse container_response;
  container_request.set_file_path(file_path);
  container_request.set_package_id(package_id);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status = garcon_stub_->InstallLinuxPackage(
      &ctx, container_request, &container_response);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to install Linux package in container " << name_
               << ": " << status.error_message()
               << " code: " << status.error_code();
    out_error->assign("gRPC failure installing Linux package in container: " +
                      status.error_message());
    return vm_tools::container::InstallLinuxPackageResponse::FAILED;
  }
  out_error->assign(container_response.failure_reason());
  return container_response.status();
}

vm_tools::container::UninstallPackageOwningFileResponse::Status
Container::UninstallPackageOwningFile(const std::string& desktop_file_id,
                                      std::string* out_error) {
  vm_tools::container::UninstallPackageOwningFileRequest container_request;
  vm_tools::container::UninstallPackageOwningFileResponse container_response;
  container_request.set_desktop_file_id(desktop_file_id);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status = garcon_stub_->UninstallPackageOwningFile(
      &ctx, container_request, &container_response);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to uninstall package in container " << name_ << ": "
               << status.error_message() << " code: " << status.error_code();
    out_error->assign("gRPC failure uninstalling package in container: " +
                      status.error_message());
    return vm_tools::container::UninstallPackageOwningFileResponse::FAILED;
  }
  out_error->assign(container_response.failure_reason());
  return container_response.status();
}

bool Container::AppSearch(const std::string& query,
                          std::vector<std::string>* out_package_names,
                          std::string* out_error) {
  CHECK(out_package_names);
  CHECK(out_error);
  vm_tools::container::AppSearchRequest container_request;
  vm_tools::container::AppSearchResponse container_response;
  container_request.set_query(query);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status =
      garcon_stub_->AppSearch(&ctx, container_request, &container_response);
  if (!status.ok()) {
    out_error->assign("Failed AppSearch with query '" + query +
                      "' in container " + name_ + ": " +
                      status.error_message());
    LOG(ERROR) << *out_error;
    return false;
  }
  for (auto& package : container_response.packages())
    out_package_names->push_back(package.package_name());
  return true;
}

}  // namespace cicerone
}  // namespace vm_tools
