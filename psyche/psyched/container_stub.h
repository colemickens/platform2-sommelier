// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_CONTAINER_STUB_H_
#define PSYCHE_PSYCHED_CONTAINER_STUB_H_

#include "psyche/psyched/container.h"

#include <memory>
#include <string>

#include <base/macros.h>

namespace psyche {

class ServiceStub;

// Stub implementation of ClientInterface used by tests.
class ContainerStub : public ContainerInterface {
 public:
  explicit ContainerStub(const std::string& container_name);
  ~ContainerStub() override;

  int launch_count() const { return launch_count_; }
  void set_launch_return_value(bool value) {
    launch_return_value_ = value;
  }

  // Adds a ServiceStub named |service_name| to |services_| and returns a
  // pointer to it. Ownership of the stub remains with this class.
  ServiceStub* AddService(const std::string& service_name);

  // ContainerInterface:
  std::string GetName() const override;
  const ServiceMap& GetServices() const override;
  bool Launch() override;

 private:
  std::string name_;
  ServiceMap services_;

  // Number of times that Launch() has been called.
  int launch_count_;

  // The return value of Launch().
  bool launch_return_value_;

  DISALLOW_COPY_AND_ASSIGN(ContainerStub);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_CONTAINER_STUB_H_
