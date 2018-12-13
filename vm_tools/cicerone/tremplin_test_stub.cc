// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/cicerone/tremplin_test_stub.h"

namespace vm_tools {
namespace cicerone {

TremplinTestStub::TremplinTestStub() = default;
TremplinTestStub::~TremplinTestStub() = default;

void TremplinTestStub::SetCreateContainerReturn(const grpc::Status& status) {
  base::AutoLock lock_scope(lock_);
  create_container_return_ = status;
}
void TremplinTestStub::SetCreateContainerResponse(
    const vm_tools::tremplin::CreateContainerResponse& response) {
  base::AutoLock lock_scope(lock_);
  create_container_response_ = response;
}
void TremplinTestStub::SetStartContainerReturn(const grpc::Status& status) {
  base::AutoLock lock_scope(lock_);
  start_container_return_ = status;
}
void TremplinTestStub::SetStartContainerResponse(
    const vm_tools::tremplin::StartContainerResponse& response) {
  base::AutoLock lock_scope(lock_);
  start_container_response_ = response;
}
void TremplinTestStub::SetGetContainerUsernameReturn(
    const grpc::Status& status) {
  base::AutoLock lock_scope(lock_);
  get_container_username_return_ = status;
}
void TremplinTestStub::SetGetContainerUsernameResponse(
    const vm_tools::tremplin::GetContainerUsernameResponse& response) {
  base::AutoLock lock_scope(lock_);
  get_container_username_response_ = response;
}
void TremplinTestStub::SetSetUpUserReturn(const grpc::Status& status) {
  base::AutoLock lock_scope(lock_);
  set_up_user_return_ = status;
}
void TremplinTestStub::SetSetUpUserResponse(
    const vm_tools::tremplin::SetUpUserResponse& response) {
  base::AutoLock lock_scope(lock_);
  set_up_user_response_ = response;
}
void TremplinTestStub::SetGetContainerInfoReturn(const grpc::Status& status) {
  base::AutoLock lock_scope(lock_);
  get_container_info_return_ = status;
}
void TremplinTestStub::SetGetContainerInfoResponse(
    const vm_tools::tremplin::GetContainerInfoResponse& response) {
  base::AutoLock lock_scope(lock_);
  get_container_info_response_ = response;
}
void TremplinTestStub::SetSetTimezoneReturn(const grpc::Status& status) {
  base::AutoLock lock_scope(lock_);
  set_timezone_return_ = status;
}
void TremplinTestStub::SetSetTimezoneResponse(
    const vm_tools::tremplin::SetTimezoneResponse& response) {
  base::AutoLock lock_scope(lock_);
  set_timezone_response_ = response;
}

grpc::Status TremplinTestStub::CreateContainer(
    grpc::ServerContext* ctx,
    const vm_tools::tremplin::CreateContainerRequest* request,
    vm_tools::tremplin::CreateContainerResponse* response) {
  base::AutoLock lock_scope(lock_);
  *response = create_container_response_;
  grpc::Status return_status =
      create_container_return_;  // Copy while still holding |lock_|.
  return return_status;
}

grpc::Status TremplinTestStub::StartContainer(
    grpc::ServerContext* ctx,
    const vm_tools::tremplin::StartContainerRequest* request,
    vm_tools::tremplin::StartContainerResponse* response) {
  base::AutoLock lock_scope(lock_);
  *response = start_container_response_;
  grpc::Status return_status =
      start_container_return_;  // Copy while still holding |lock_|.
  return return_status;
}

grpc::Status TremplinTestStub::GetContainerUsername(
    grpc::ServerContext* ctx,
    const vm_tools::tremplin::GetContainerUsernameRequest* request,
    vm_tools::tremplin::GetContainerUsernameResponse* response) {
  base::AutoLock lock_scope(lock_);
  *response = get_container_username_response_;
  grpc::Status return_status =
      get_container_username_return_;  // Copy while still holding |lock_|.
  return return_status;
}

grpc::Status TremplinTestStub::SetUpUser(
    grpc::ServerContext* ctx,
    const vm_tools::tremplin::SetUpUserRequest* request,
    vm_tools::tremplin::SetUpUserResponse* response) {
  base::AutoLock lock_scope(lock_);
  *response = set_up_user_response_;
  grpc::Status return_status =
      set_up_user_return_;  // Copy while still holding |lock_|.
  return return_status;
}

grpc::Status TremplinTestStub::GetContainerInfo(
    grpc::ServerContext* ctx,
    const vm_tools::tremplin::GetContainerInfoRequest* request,
    vm_tools::tremplin::GetContainerInfoResponse* response) {
  base::AutoLock lock_scope(lock_);
  *response = get_container_info_response_;
  grpc::Status return_status =
      get_container_info_return_;  // Copy while still holding |lock_|.
  return return_status;
}

grpc::Status TremplinTestStub::SetTimezone(
    grpc::ServerContext* ctx,
    const vm_tools::tremplin::SetTimezoneRequest* request,
    vm_tools::tremplin::SetTimezoneResponse* response) {
  base::AutoLock lock_scope(lock_);
  *response = set_timezone_response_;
  grpc::Status return_status =
      set_timezone_return_;  // Copy while still holding |lock_|.
  return return_status;
}

}  // namespace cicerone
}  // namespace vm_tools
