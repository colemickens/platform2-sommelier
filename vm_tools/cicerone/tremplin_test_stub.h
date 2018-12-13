// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CICERONE_TREMPLIN_TEST_STUB_H_
#define VM_TOOLS_CICERONE_TREMPLIN_TEST_STUB_H_

#include <base/synchronization/lock.h>

#include "tremplin.grpc.pb.h"  // NOLINT(build/include)

namespace vm_tools {
namespace cicerone {

// Implements a simple stub for the Tremplin service for testing.
class TremplinTestStub final : public vm_tools::tremplin::Tremplin::Service {
 public:
  TremplinTestStub();
  ~TremplinTestStub() override;

  // Controls for the stub. All return status default to OK, all proto
  // responses default to blank.
  void SetCreateContainerReturn(const grpc::Status& status);
  void SetCreateContainerResponse(
      const vm_tools::tremplin::CreateContainerResponse& response);
  void SetStartContainerReturn(const grpc::Status& status);
  void SetStartContainerResponse(
      const vm_tools::tremplin::StartContainerResponse& response);
  void SetGetContainerUsernameReturn(const grpc::Status& status);
  void SetGetContainerUsernameResponse(
      const vm_tools::tremplin::GetContainerUsernameResponse& response);
  void SetSetUpUserReturn(const grpc::Status& status);
  void SetSetUpUserResponse(
      const vm_tools::tremplin::SetUpUserResponse& response);
  void SetGetContainerInfoReturn(const grpc::Status& status);
  void SetGetContainerInfoResponse(
      const vm_tools::tremplin::GetContainerInfoResponse& response);
  void SetSetTimezoneReturn(const grpc::Status& status);
  void SetSetTimezoneResponse(
      const vm_tools::tremplin::SetTimezoneResponse& response);

  // vm_tools::tremplin::Tremplin::Service overrides:
  grpc::Status CreateContainer(
      grpc::ServerContext* ctx,
      const vm_tools::tremplin::CreateContainerRequest* request,
      vm_tools::tremplin::CreateContainerResponse* response) override;

  grpc::Status StartContainer(
      grpc::ServerContext* ctx,
      const vm_tools::tremplin::StartContainerRequest* request,
      vm_tools::tremplin::StartContainerResponse* response) override;

  grpc::Status GetContainerUsername(
      grpc::ServerContext* ctx,
      const vm_tools::tremplin::GetContainerUsernameRequest* request,
      vm_tools::tremplin::GetContainerUsernameResponse* response) override;

  grpc::Status SetUpUser(
      grpc::ServerContext* ctx,
      const vm_tools::tremplin::SetUpUserRequest* request,
      vm_tools::tremplin::SetUpUserResponse* response) override;

  grpc::Status GetContainerInfo(
      grpc::ServerContext* ctx,
      const vm_tools::tremplin::GetContainerInfoRequest* request,
      vm_tools::tremplin::GetContainerInfoResponse* response) override;

  grpc::Status SetTimezone(
      grpc::ServerContext* ctx,
      const vm_tools::tremplin::SetTimezoneRequest* request,
      vm_tools::tremplin::SetTimezoneResponse* response) override;

 private:
  // |lock_| controls access to all other variables.
  base::Lock lock_;

  grpc::Status create_container_return_;
  vm_tools::tremplin::CreateContainerResponse create_container_response_;
  grpc::Status start_container_return_;
  vm_tools::tremplin::StartContainerResponse start_container_response_;
  grpc::Status get_container_username_return_;
  vm_tools::tremplin::GetContainerUsernameResponse
      get_container_username_response_;
  grpc::Status set_up_user_return_;
  vm_tools::tremplin::SetUpUserResponse set_up_user_response_;
  grpc::Status get_container_info_return_;
  vm_tools::tremplin::GetContainerInfoResponse get_container_info_response_;
  grpc::Status set_timezone_return_;
  vm_tools::tremplin::SetTimezoneResponse set_timezone_response_;
};

}  // namespace cicerone
}  // namespace vm_tools

#endif  // VM_TOOLS_CICERONE_TREMPLIN_TEST_STUB_H_
