// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/stub_factory.h"

#include <utility>

#include <protobinder/binder_proxy.h>
#include <soma/read_only_container_spec.h>

#include "psyche/proto_bindings/soma_container_spec.pb.h"
#include "psyche/psyched/client_stub.h"
#include "psyche/psyched/container_stub.h"
#include "psyche/psyched/service_stub.h"

using protobinder::BinderProxy;

namespace psyche {

StubFactory::StubFactory() = default;

StubFactory::~StubFactory() = default;

ContainerStub* StubFactory::GetContainer(const std::string& container_name) {
  auto const it = containers_.find(container_name);
  return it != containers_.end() ? it->second : nullptr;
}

ServiceStub* StubFactory::GetService(const std::string& service_name) {
  auto const it = services_.find(service_name);
  return it != services_.end() ? it->second : nullptr;
}

ClientStub* StubFactory::GetClient(const BinderProxy& client_proxy) {
  auto const it = clients_.find(client_proxy.handle());
  return it != clients_.end() ? it->second : nullptr;
}

void StubFactory::SetContainer(const std::string& container_name,
                               std::unique_ptr<ContainerStub> container) {
  new_containers_[container_name] = std::move(container);
}

std::unique_ptr<ContainerInterface> StubFactory::CreateContainer(
    const soma::ContainerSpec& spec) {
  soma::ReadOnlyContainerSpec spec_reader(&spec);
  const std::string container_name = spec_reader.name();
  std::unique_ptr<ContainerStub> container;
  auto it = new_containers_.find(container_name);
  if (it != new_containers_.end()) {
    container = std::move(it->second);
    new_containers_.erase(it);
  } else {
    container.reset(new ContainerStub(container_name));
    soma::ReadOnlyContainerSpec spec_reader(&spec);
    for (const auto& service_name : spec_reader.service_names())
      container->AddService(service_name);
  }

  containers_[container->GetName()] = container.get();
  return std::unique_ptr<ContainerInterface>(container.release());
}

std::unique_ptr<ServiceInterface> StubFactory::CreateService(
    const std::string& name) {
  ServiceStub* service = new ServiceStub(name);
  services_[name] = service;
  return std::unique_ptr<ServiceInterface>(service);
}

std::unique_ptr<ClientInterface> StubFactory::CreateClient(
    std::unique_ptr<BinderProxy> client_proxy) {
  const uint32_t handle = client_proxy->handle();
  ClientStub* client = new ClientStub(std::move(client_proxy));
  clients_[handle] = client;
  return std::unique_ptr<ClientInterface>(client);
}

}  // namespace psyche
