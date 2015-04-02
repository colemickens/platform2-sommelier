// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_CONTAINER_H_
#define PSYCHE_PSYCHED_CONTAINER_H_

#include <map>
#include <memory>
#include <string>

#include <base/macros.h>
#include <soma/read_only_container_spec.h>

#include "psyche/proto_bindings/soma_container_spec.pb.h"
#include "psyche/psyched/service_observer.h"

namespace psyche {

class FactoryInterface;
class ServiceInterface;

// Corresponds to a ContainerSpec returned by soma and launched one or more
// times by germ. This class persists across multiple launches of the container.
class ContainerInterface {
 public:
  // Keyed by service name.
  using ServiceMap = std::map<std::string, std::unique_ptr<ServiceInterface>>;

  virtual ~ContainerInterface() = default;

  // Returns this container's name.
  virtual std::string GetName() const = 0;

  // Returns the services provided by this container. Binder proxies for these
  // services have not necessarily been registered yet.
  virtual const ServiceMap& GetServices() const = 0;

  // Launches the container.
  virtual void Launch() = 0;
};

// The real implementation of ContainerInterface.
class Container : public ContainerInterface, public ServiceObserver {
 public:
  // |factory| is used to construct ServiceInterface objects, permitting tests
  // to create stub services instead.
  Container(const soma::ContainerSpec& spec, FactoryInterface* factory);
  ~Container() override;

  // ContainerInterface:
  std::string GetName() const override;
  const ServiceMap& GetServices() const override;
  void Launch() override;

  // ServiceObserver:
  void OnServiceStateChange(ServiceInterface* service) override;

 private:
  // The specification describing this container.
  soma::ContainerSpec spec_;

  // Wrapper around |spec_|.
  soma::ReadOnlyContainerSpec spec_reader_;

  // Services that are provided by this container. These are created when the
  // service is created; the binder proxies that are given to clients are set
  // later when the services are registered.
  ServiceMap services_;

  DISALLOW_COPY_AND_ASSIGN(Container);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_CONTAINER_H_
