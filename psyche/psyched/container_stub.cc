// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/container_stub.h"

#include "psyche/proto_bindings/soma_container_spec.pb.h"
#include "psyche/psyched/service.h"
#include "psyche/psyched/service_stub.h"

using soma::ContainerSpec;

namespace psyche {

ContainerStub::ContainerStub(const std::string& container_name)
    : name_(container_name), launch_count_(0) {}

ContainerStub::~ContainerStub() = default;

ServiceStub* ContainerStub::AddService(const std::string& service_name) {
  ServiceStub* service = new ServiceStub(service_name);
  services_[service_name] = std::unique_ptr<ServiceInterface>(service);
  return service;
}

std::string ContainerStub::GetName() const {
  return name_;
}

const ContainerInterface::ServiceMap& ContainerStub::GetServices() const {
  return services_;
}

void ContainerStub::Launch() {
  launch_count_++;
}

}  // namespace psyche
