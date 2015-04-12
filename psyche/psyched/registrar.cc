// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/registrar.h"

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <germ/constants.h>
#include <protobinder/binder_manager.h>
#include <protobinder/binder_proxy.h>
#include <protobinder/iservice_manager.h>
#include <protobinder/proto_util.h>
#include <soma/constants.h>

#include "psyche/common/constants.h"
#include "psyche/proto_bindings/soma_container_spec.pb.h"
#include "psyche/psyched/client.h"
#include "psyche/psyched/container.h"
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
  RealFactory() = default;
  ~RealFactory() override = default;

  // FactoryInterface:
  std::unique_ptr<ContainerInterface> CreateContainer(
      const soma::ContainerSpec& spec) override {
    return std::unique_ptr<ContainerInterface>(new Container(spec, this));
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
    // TODO(mcolagrosso): Add GermConnection ptr to RealFactory to use when
    // constructing containers.
    factory_.reset(new RealFactory());
}

int Registrar::RegisterService(RegisterServiceRequest* in,
                               RegisterServiceResponse* out) {
  const std::string service_name = in->name();
  std::unique_ptr<BinderProxy> proxy =
      protobinder::ExtractBinderFromProto(in->mutable_binder());
  LOG(INFO) << "Got request to register \"" << service_name << "\" with "
            << "handle " << proxy->handle();

  if (service_name.empty()) {
    LOG(WARNING) << "Ignoring request to register service with invalid name";
    out->set_success(false);
    return 0;
  }

  if (service_name == soma::kSomaServiceName) {
    soma_->SetProxy(std::move(proxy));
    // TODO(derat): Fetch and start persistent containers.
    out->set_success(true);
    return 0;
  } else if (service_name == germ::kGermServiceName) {
    germ_->SetProxy(std::move(proxy));
    out->set_success(true);
    return 0;
  }

  ServiceInterface* service =
      GetService(service_name, false /* create_container */);
  if (service) {
    // The service is already known, but maybe its proxy wasn't registered or
    // has died.
    if (service->GetProxy()) {
      LOG(WARNING) << "Ignoring request to register already-registered "
                   << "service \"" << service_name << "\"";
      out->set_success(false);
      return 0;
    }
  } else {
    // This service wasn't already registered or claimed by a container that we
    // launched. Go ahead and create a new object to track it.
    // TODO(derat): Don't allow non-container services after everything is
    // running within containers.
    const auto& it = non_container_services_.emplace(
        service_name, factory_->CreateService(service_name)).first;
    service = it->second.get();
    services_.emplace(service_name, service);
  }
  DCHECK(service);
  service->SetProxy(std::move(proxy));

  out->set_success(true);
  return 0;
}

int Registrar::RequestService(RequestServiceRequest* in) {
  const std::string service_name = in->name();
  std::unique_ptr<BinderProxy> client_proxy =
      protobinder::ExtractBinderFromProto(in->mutable_client_binder());
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
      GetService(service_name, true /* create_container */);
  if (!service) {
    LOG(WARNING) << "Service \"" << service_name << "\" is unknown";
    client->ReportServiceRequestFailure(service_name);
    // TODO(derat): Drop the client immediately if it doesn't have any other
    // services? This would require updating some tests which currently inspect
    // the client after calling this method to check if failure was reported.
    return 0;
  }

  // Check that the client didn't previously request this service.
  if (!service->HasClient(client)) {
    service->AddClient(client);
    client->AddService(service);
  }

  return 0;
}

ServiceInterface* Registrar::GetService(const std::string& service_name,
                                        bool create_container) {
  auto it = services_.find(service_name);
  if (it != services_.end())
    return it->second;

  if (!create_container)
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

  std::unique_ptr<ContainerInterface> container =
      factory_->CreateContainer(spec);
  const std::string container_name = container->GetName();

  if (!container->GetServices().count(service_name)) {
    // This happens if we get a request for a service that doesn't exist that's
    // in a service namespace that _does_ exist.
    LOG(WARNING) << "Container \"" << container_name << "\" doesn't provide "
                 << "service \"" << service_name << "\"";
    return nullptr;
  }
  if (containers_.count(container_name)) {
    // This means that somad for some reason returned this ContainerSpec
    // earlier, but it didn't previously list the service that we're looking for
    // now.
    LOG(WARNING) << "Container \"" << container_name << "\" already exists";
    return nullptr;
  }
  for (const auto& service_it : container->GetServices()) {
    if (services_.count(service_it.first)) {
      // This means that somad didn't validate that a ContainerSpec doesn't list
      // any services outside of its service namespace, or that this service was
      // already registered in |non_container_services_|.
      LOG(WARNING) << "Container \"" << container_name << "\" provides already-"
                   << "known service \"" << service_it.first << "\"";
      return nullptr;
    }
  }

  // TODO(mcolagrosso): Add return value to Launch() and check it here.
  container->Launch();
  LOG(INFO) << "Created container \"" << container_name << "\"";

  for (const auto& service_it : container->GetServices())
    services_[service_it.first] = service_it.second.get();
  it = services_.find(service_name);
  CHECK(it != services_.end());
  ServiceInterface* service = it->second;

  containers_.emplace(container_name, std::move(container));

  return service;
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
