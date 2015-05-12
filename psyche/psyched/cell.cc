// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/cell.h"

#include <utility>

#include "psyche/psyched/factory_interface.h"
#include "psyche/psyched/germ_connection.h"
#include "psyche/psyched/service.h"

using soma::ContainerSpec;

namespace psyche {

namespace {

// How long to wait for services to register themselves.
const int kServiceWaitTimeSec = 20;

}  // namespace

Cell::TestApi::TestApi(Cell* cell) : cell_(cell) {}

Cell::TestApi::~TestApi() {}

bool Cell::TestApi::TriggerVerifyServicesTimeout() {
  if (!cell_->verify_services_timer_.IsRunning())
    return false;

  cell_->verify_services_timer_.Stop();
  cell_->VerifyServicesRegistered();
  return true;
}

Cell::Cell(const ContainerSpec& spec,
           FactoryInterface* factory,
           GermConnection* germ)
    : spec_(spec), germ_connection_(germ) {
  DCHECK(factory);
  for (const auto& name : spec_.service_names()) {
    std::unique_ptr<ServiceInterface> service(factory->CreateService(name));
    service->AddObserver(this);
    services_[name] = std::move(service);
  }
}

Cell::~Cell() {
  if (init_pid_ >= 0)
    Terminate();
}

std::string Cell::GetName() const {
  return spec_.name();
}

const Cell::ServiceMap& Cell::GetServices() const {
  return services_;
}

bool Cell::Launch() {
  for (const auto& service_it : services_) {
    service_it.second->OnCellLaunched();
  }
  verify_services_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kServiceWaitTimeSec), this,
      &Cell::VerifyServicesRegistered);
  return germ_connection_->Launch(spec_) == GermConnection::Result::SUCCESS;
}

void Cell::OnServiceProxyChange(ServiceInterface* service) {
  CHECK(services_.count(service->GetName()))
      << "Cell \"" << GetName() << "\" received proxy change notification "
      << "for unexpected service \"" << service->GetName() << "\"";

  if (!service->GetProxy()) {
    LOG(INFO) << "Proxy for service \"" << service->GetName() << "\" within "
              << "\"" << GetName() << "\" died; notifying clients.";
    service->OnServiceUnavailable();
  } else if (AllServicesRegistered()) {
    verify_services_timer_.Stop();
  }
}

bool Cell::Terminate() {
  DCHECK_GE(init_pid_, 0);
  verify_services_timer_.Stop();
  bool success =
      germ_connection_->Terminate(GetName()) == GermConnection::Result::SUCCESS;
  init_pid_ = -1;
  return success;
}

void Cell::VerifyServicesRegistered() {
  for (const auto& service_it : services_) {
    if (!service_it.second->GetProxy()) {
      service_it.second->OnServiceUnavailable();
    }
  }
}

bool Cell::AllServicesRegistered() {
  for (const auto& service_it : GetServices()) {
    if (!service_it.second->GetProxy()) {
      return false;
    }
  }
  return true;
}

}  // namespace psyche
