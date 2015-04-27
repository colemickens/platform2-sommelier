// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/cell_stub.h"

#include "psyche/proto_bindings/soma_container_spec.pb.h"
#include "psyche/psyched/service.h"
#include "psyche/psyched/service_stub.h"

using soma::ContainerSpec;

namespace psyche {

CellStub::CellStub(const std::string& cell_name)
    : name_(cell_name),
      launch_count_(0),
      launch_return_value_(true),
      terminate_count_(0),
      terminate_return_value_(true) {
}

CellStub::~CellStub() = default;

ServiceStub* CellStub::AddService(const std::string& service_name) {
  ServiceStub* service = new ServiceStub(service_name);
  services_[service_name] = std::unique_ptr<ServiceInterface>(service);
  return service;
}

std::string CellStub::GetName() const {
  return name_;
}

const CellInterface::ServiceMap& CellStub::GetServices() const {
  return services_;
}

bool CellStub::Launch() {
  launch_count_++;
  return launch_return_value_;
}

bool CellStub::Terminate() {
  terminate_count_++;
  return terminate_return_value_;
}

}  // namespace psyche
