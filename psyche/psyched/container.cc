// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/container.h"

#include <utility>

#include "psyche/psyched/factory_interface.h"
#include "psyche/psyched/service.h"

using soma::ContainerSpec;

namespace psyche {

Container::Container(const ContainerSpec& spec, FactoryInterface* factory)
    : spec_(spec), spec_reader_(&spec) {
  DCHECK(factory);
  for (const auto& name : spec_reader_.service_names()) {
    std::unique_ptr<ServiceInterface> service(factory->CreateService(name));
    service->AddObserver(this);
    services_[name] = std::move(service);
  }
}

Container::~Container() = default;

std::string Container::GetName() const {
  return spec_reader_.name();
}

const ContainerInterface::ServiceMap& Container::GetServices() const {
  return services_;
}

void Container::Launch() {
  // TODO(derat): Ask germ to launch |spec_|.
  NOTIMPLEMENTED();
}

void Container::OnServiceStateChange(ServiceInterface* service) {
  CHECK(services_.count(service->GetName()))
      << "Container \"" << GetName() << "\" received state change notification "
      << "for unexpected service \"" << service->GetName() << "\"";

  if (service->GetState() == ServiceInterface::State::STOPPED) {
    LOG(INFO) << "Service \"" << service->GetName() << "\" within container \""
              << GetName() << "\" stopped; relaunching container";
    Launch();
  }
}

}  // namespace psyche
