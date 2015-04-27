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

Cell::Cell(const ContainerSpec& spec,
           FactoryInterface* factory,
           GermConnection* germ)
    : spec_(spec), germ_connection_(germ), init_pid_(-1) {
  DCHECK(factory);
  for (const auto& name : spec_.service_names()) {
    std::unique_ptr<ServiceInterface> service(factory->CreateService(name));
    service->AddObserver(this);
    services_[name] = std::move(service);
  }
}

Cell::~Cell() = default;

std::string Cell::GetName() const {
  return spec_.name();
}

const Cell::ServiceMap& Cell::GetServices() const {
  return services_;
}

bool Cell::Launch() {
  return germ_connection_->Launch(spec_, &init_pid_) ==
      GermConnection::Result::SUCCESS;
}

bool Cell::Terminate() {
  return germ_connection_->Terminate(init_pid_) ==
      GermConnection::Result::SUCCESS;
}

void Cell::OnServiceProxyChange(ServiceInterface* service) {
  CHECK(services_.count(service->GetName()))
      << "Cell \"" << GetName() << "\" received proxy change notification "
      << "for unexpected service \"" << service->GetName() << "\"";

  if (!service->GetProxy()) {
    LOG(INFO) << "Proxy for service \"" << service->GetName() << "\" within "
              << "\"" << GetName() << "\" died; relaunching cell";
    Launch();
  }
}

}  // namespace psyche
