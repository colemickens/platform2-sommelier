// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_GARCON_SERVICE_IMPL_H_
#define VM_TOOLS_GARCON_SERVICE_IMPL_H_

#include <memory>

#include <base/macros.h>
#include <grpcpp/grpcpp.h>
#include <string>
#include <vector>

#include "container_guest.grpc.pb.h"  // NOLINT(build/include)

namespace vm_tools {
namespace garcon {

class PackageKitProxy;

// Actually implements the garcon service.
class ServiceImpl final : public vm_tools::container::Garcon::Service {
 public:
  explicit ServiceImpl(PackageKitProxy* package_kit_proxy);
  ~ServiceImpl() override = default;

  // Garcon::Service overrides.
  grpc::Status LaunchApplication(
      grpc::ServerContext* ctx,
      const vm_tools::container::LaunchApplicationRequest* request,
      vm_tools::container::LaunchApplicationResponse* response) override;

  grpc::Status GetIcon(grpc::ServerContext* ctx,
                       const vm_tools::container::IconRequest* request,
                       vm_tools::container::IconResponse* response) override;

  grpc::Status LaunchVshd(
      grpc::ServerContext* ctx,
      const vm_tools::container::LaunchVshdRequest* request,
      vm_tools::container::LaunchVshdResponse* response) override;

  grpc::Status GetLinuxPackageInfo(
      grpc::ServerContext* ctx,
      const vm_tools::container::LinuxPackageInfoRequest* request,
      vm_tools::container::LinuxPackageInfoResponse* response) override;

  grpc::Status InstallLinuxPackage(
      grpc::ServerContext* ctx,
      const vm_tools::container::InstallLinuxPackageRequest* request,
      vm_tools::container::InstallLinuxPackageResponse* response) override;

  grpc::Status UninstallPackageOwningFile(
      grpc::ServerContext* ctx,
      const vm_tools::container::UninstallPackageOwningFileRequest* request,
      vm_tools::container::UninstallPackageOwningFileResponse* response)
      override;

  grpc::Status GetDebugInformation(
      grpc::ServerContext* ctx,
      const vm_tools::container::GetDebugInformationRequest* request,
      vm_tools::container::GetDebugInformationResponse* response) override;

  grpc::Status AppSearch(
      grpc::ServerContext* ctx,
      const vm_tools::container::AppSearchRequest* request,
      vm_tools::container::AppSearchResponse* response) override;

 private:
  PackageKitProxy* package_kit_proxy_;  // Not owned.
  // Stores the names of packages that match search constraints
  std::vector<std::string> valid_packages_;

  DISALLOW_COPY_AND_ASSIGN(ServiceImpl);
};

}  // namespace garcon
}  // namespace vm_tools

#endif  // VM_TOOLS_GARCON_SERVICE_IMPL_H_
