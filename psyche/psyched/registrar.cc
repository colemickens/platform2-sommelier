// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/registrar.h"

#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <germ/constants.h>
#include <protobinder/binder_manager.h>
#include <protobinder/binder_proxy.h>
#include <protobinder/iservice_manager.h>
#include <soma/constants.h>

#include "psyche/common/constants.h"
#include "psyche/proto_bindings/soma_container_spec.pb.h"
#include "psyche/psyched/cell.h"
#include "psyche/psyched/client.h"
#include "psyche/psyched/factory_interface.h"
#include "psyche/psyched/germ_connection.h"
#include "psyche/psyched/service.h"
#include "psyche/psyched/soma_connection.h"

using protobinder::BinderManager;
using protobinder::BinderProxy;

namespace psyche {
namespace {

// Implementation of FactoryInterface that returns real objects.
class RealFactory : public FactoryInterface {
 public:
  explicit RealFactory(GermConnection* germ) : germ_connection_(germ) {}
  ~RealFactory() override = default;

  // FactoryInterface:
  std::unique_ptr<CellInterface> CreateCell(
      const soma::ContainerSpec& spec) override {
    return std::unique_ptr<CellInterface>(
        new Cell(spec, this, germ_connection_));
  }
  std::unique_ptr<ServiceInterface> CreateService(
      const std::string& name) override {
    return std::unique_ptr<ServiceInterface>(new Service(name));
  }
  std::unique_ptr<ClientInterface> CreateClient(
      std::unique_ptr<BinderProxy> client_proxy) override {
    return std::unique_ptr<ClientInterface>(
        new Client(std::move(client_proxy)));
  }

 private:
  GermConnection* germ_connection_;
  DISALLOW_COPY_AND_ASSIGN(RealFactory);
};

}  // namespace

Registrar::Registrar()
    : soma_(new SomaConnection),
      germ_(new GermConnection),
      weak_ptr_factory_(this) {}

Registrar::~Registrar() = default;

void Registrar::SetFactoryForTesting(
    std::unique_ptr<FactoryInterface> factory) {
  CHECK(!factory_);
  factory_ = std::move(factory);
}

void Registrar::Init() {
  if (!factory_)
    factory_.reset(new RealFactory(germ_.get()));
}

Status Registrar::RegisterService(RegisterServiceRequest* in,
                                  RegisterServiceResponse* out) {
  const std::string service_name = in->name();
  std::unique_ptr<BinderProxy> proxy(
      new BinderProxy(in->binder().proxy_handle()));
  LOG(INFO) << "Got request to register \"" << service_name << "\" with "
            << "handle " << proxy->handle();

  if (service_name.empty()) {
    return STATUS_APP_ERROR_LOG(
        logging::LOG_WARNING, RegisterServiceResponse::INVALID_NAME,
        "Ignoring request to register service with invalid name");
  }

  if (service_name == soma::kSomaServiceName) {
    const bool was_registered = soma_->HasProxy();
    soma_->SetProxy(std::move(proxy));
    // Only create persistent cells the first time somad is registered -- assume
    // that the specs are the same if it crashes and gets restarted.
    if (!was_registered)
      CreatePersistentCells();
    return STATUS_OK();
  } else if (service_name == germ::kGermServiceName) {
    germ_->SetProxy(std::move(proxy));
    return STATUS_OK();
  }

  ServiceInterface* service =
      GetService(service_name, false /* create_cell */);
  if (service) {
    // The service is already known, but maybe its proxy wasn't registered or
    // has died.
    if (service->GetProxy()) {
      return STATUS_APP_ERROR_LOG(
          logging::LOG_WARNING, RegisterServiceResponse::ALREADY_REGISTERED,
          "Ignoring request to register already-registered service " +
              service_name);
    }
  } else {
    // This service wasn't already registered or claimed by a cell that we
    // launched. Go ahead and create a new object to track it.
    // TODO(derat): Don't allow non-cell services after everything is
    // running within cells.
    const auto& it = non_cell_services_.emplace(
        service_name, factory_->CreateService(service_name)).first;
    service = it->second.get();
    services_.emplace(service_name, service);
  }
  DCHECK(service);
  service->SetProxy(std::move(proxy));

  return STATUS_OK();
}

Status Registrar::RequestService(RequestServiceRequest* in) {
  const std::string service_name = in->name();
  std::unique_ptr<BinderProxy> client_proxy(
      new BinderProxy(in->client_binder().proxy_handle()));
  int32_t client_handle = client_proxy->handle();
  LOG(INFO) << "Got request to provide service \"" << service_name << "\""
            << " to client with handle " << client_handle;

  auto client_it = clients_.find(client_handle);
  if (client_it == clients_.end()) {
    // We didn't already know about the client.
    client_proxy->SetDeathCallback(base::Bind(
        &Registrar::HandleClientBinderDeath,
        weak_ptr_factory_.GetWeakPtr(), client_handle));
    client_it = clients_.emplace(
        client_handle, factory_->CreateClient(std::move(client_proxy))).first;
  }
  ClientInterface* client = client_it->second.get();

  ServiceInterface* service =
      GetService(service_name, true /* create_cell */);
  if (!service) {
    LOG(WARNING) << "Service \"" << service_name << "\" is unknown";
    client->ReportServiceRequestFailure(service_name);
    // TODO(derat): Drop the client immediately if it doesn't have any other
    // services? This would require updating some tests which currently inspect
    // the client after calling this method to check if failure was reported.
    return STATUS_OK();
  }

  // Check that the client didn't previously request this service.
  if (!service->HasClient(client)) {
    service->AddClient(client);
    client->AddService(service);
  }

  return STATUS_OK();
}

bool Registrar::AddCell(std::unique_ptr<CellInterface> cell) {
  const std::string cell_name = cell->GetName();

  if (cells_.count(cell_name)) {
    // This means that somad for some reason returned this ContainerSpec
    // earlier, but it didn't previously list the service that we're looking for
    // now.
    LOG(WARNING) << "Cell \"" << cell_name << "\" already exists";
    return false;
  }

  for (const auto& service_it : cell->GetServices()) {
    if (services_.count(service_it.first)) {
      // This means that somad didn't validate that a ContainerSpec doesn't list
      // any services outside of its service namespace, or that this service was
      // already registered in |non_cell_services_|.
      LOG(WARNING) << "Cell \"" << cell_name << "\" provides already-known "
                   << "service \"" << service_it.first << "\"";
      return false;
    }
  }

  if (!cell->Launch()) {
    LOG(WARNING) << "Cell \"" << cell_name << "\" failed to launch";
    return false;
  }

  for (const auto& service_it : cell->GetServices())
    services_[service_it.first] = service_it.second.get();
  cells_.emplace(cell_name, std::move(cell));
  return true;
}

ServiceInterface* Registrar::GetService(const std::string& service_name,
                                        bool create_cell) {
  auto it = services_.find(service_name);
  if (it != services_.end())
    return it->second;

  if (!create_cell)
    return nullptr;

  soma::ContainerSpec spec;
  const SomaConnection::Result result =
      soma_->GetContainerSpecForService(service_name, &spec);
  if (result != SomaConnection::Result::SUCCESS) {
    // TODO(derat): Pass back an error code so the client can be notified if the
    // service is unknown vs. this being a possibly-transient error.
    LOG(WARNING) << "Failed to get ContainerSpec for service \""
                 << service_name << "\" from soma: "
                 << SomaConnection::ResultToString(result);
    return nullptr;
  }

  std::unique_ptr<CellInterface> cell = factory_->CreateCell(spec);
  LOG(INFO) << "Created ephemeral cell \"" << cell->GetName() << "\"";

  if (!cell->GetServices().count(service_name)) {
    // This happens if we get a request for a service that doesn't exist that's
    // in a service namespace that _does_ exist.
    LOG(WARNING) << "Cell \"" << cell->GetName() << "\" doesn't "
                 << "provide service \"" << service_name << "\"";
    return nullptr;
  }

  if (!AddCell(std::move(cell)))
    return nullptr;

  it = services_.find(service_name);
  CHECK(it != services_.end());
  return it->second;
}

void Registrar::CreatePersistentCells() {
  std::vector<soma::ContainerSpec> specs;
  SomaConnection::Result result = soma_->GetPersistentContainerSpecs(&specs);
  if (result != SomaConnection::Result::SUCCESS) {
    LOG(ERROR) << "Failed to get persistent container specs: "
               << SomaConnection::ResultToString(result);
    return;
  }

  for (const auto& spec : specs) {
    std::unique_ptr<CellInterface> cell = factory_->CreateCell(spec);
    LOG(INFO) << "Created persistent cell \"" << cell->GetName() << "\"";
    AddCell(std::move(cell));
  }
}

void Registrar::HandleClientBinderDeath(int32_t handle) {
  LOG(INFO) << "Got binder death notification for client with handle "
            << handle;

  auto it = clients_.find(handle);
  if (it == clients_.end()) {
    LOG(ERROR) << "Ignoring death notification for unknown client with handle "
               << handle;
    return;
  }

  ClientInterface* client = it->second.get();
  for (auto service : client->GetServices())
    service->RemoveClient(client);

  // TODO(derat): Stop unused services?

  clients_.erase(it);
}

}  // namespace psyche
