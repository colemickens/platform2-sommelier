// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_STUB_FACTORY_H_
#define PSYCHE_PSYCHED_STUB_FACTORY_H_

#include "psyche/psyched/factory_interface.h"

#include <base/macros.h>

#include <cstdint>
#include <map>
#include <string>

namespace psyche {

class ClientStub;
class ContainerStub;
class ServiceStub;

// Implementation of FactoryInterface that just returns stub objects. Used for
// testing.
class StubFactory : public FactoryInterface {
 public:
  StubFactory();
  ~StubFactory() override;

  // Returns the last-created stub for the given identifier.
  ContainerStub* GetContainer(const std::string& container_name);
  ServiceStub* GetService(const std::string& service_name);
  ClientStub* GetClient(const protobinder::BinderProxy& client_proxy);

  // Sets the container that will be returned for a CreateContainer() call for a
  // ContainerSpec named |container_name|. If CreateContainer() is called for a
  // container not present here, a new stub will be created automatically.
  void SetContainer(const std::string& container_name,
                    std::unique_ptr<ContainerStub> container);

  // FactoryInterface:
  std::unique_ptr<ContainerInterface> CreateContainer(
      const soma::ContainerSpec& spec) override;
  std::unique_ptr<ServiceInterface> CreateService(
      const std::string& name) override;
  std::unique_ptr<ClientInterface> CreateClient(
      std::unique_ptr<protobinder::BinderProxy> client_proxy) override;

 private:
  // Containers, services, and clients that have been returned by Create*().
  // Keyed by container name, service name, and client proxy handle,
  // respectively. Objects are owned by Registrar.
  std::map<std::string, ContainerStub*> containers_;
  std::map<std::string, ServiceStub*> services_;
  std::map<uint32_t, ClientStub*> clients_;

  // Preset containers to return in response to CreateContainer() calls, keyed
  // by container name.
  std::map<std::string, std::unique_ptr<ContainerStub>> new_containers_;

  DISALLOW_COPY_AND_ASSIGN(StubFactory);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_STUB_FACTORY_H_
