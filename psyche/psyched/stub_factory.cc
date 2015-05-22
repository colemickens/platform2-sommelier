// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/stub_factory.h"

#include <utility>

#include <protobinder/binder_proxy.h>

#include "psyche/proto_bindings/soma_sandbox_spec.pb.h"
#include "psyche/psyched/cell_stub.h"
#include "psyche/psyched/client_stub.h"
#include "psyche/psyched/service_stub.h"

using protobinder::BinderProxy;

namespace psyche {

StubFactory::StubFactory() = default;

StubFactory::~StubFactory() = default;

CellStub* StubFactory::GetCell(const std::string& cell_name) {
  auto const it = cells_.find(cell_name);
  return it != cells_.end() ? it->second : nullptr;
}

ServiceStub* StubFactory::GetService(const std::string& endpoint_name) {
  auto const it = services_.find(endpoint_name);
  return it != services_.end() ? it->second : nullptr;
}

ClientStub* StubFactory::GetClient(uint32_t client_proxy_handle) {
  auto const it = clients_.find(client_proxy_handle);
  return it != clients_.end() ? it->second : nullptr;
}

void StubFactory::SetCell(const std::string& cell_name,
                          std::unique_ptr<CellStub> cell) {
  new_cells_[cell_name] = std::move(cell);
}

std::unique_ptr<CellInterface> StubFactory::CreateCell(
    const soma::SandboxSpec& spec) {
  const std::string& cell_name = spec.name();
  std::unique_ptr<CellStub> cell;
  auto it = new_cells_.find(cell_name);
  if (it != new_cells_.end()) {
    cell = std::move(it->second);
    new_cells_.erase(it);
  } else {
    cell.reset(new CellStub(cell_name));
    for (const auto& endpoint_name : spec.endpoint_names())
      cell->AddService(endpoint_name);
  }

  cells_[cell->GetName()] = cell.get();
  return std::unique_ptr<CellInterface>(cell.release());
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
