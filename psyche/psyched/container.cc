// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/container.h"

#include <utility>

#include "psyche/psyched/factory_interface.h"
#include "psyche/psyched/service.h"

using soma::ContainerSpec;

namespace psyche {

Container::Container(const ContainerSpec& spec,
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

Container::~Container() = default;

std::string Container::GetName() const {
  return spec_.name();
}

const ContainerInterface::ServiceMap& Container::GetServices() const {
  return services_;
}

void Container::Launch() {
  // TODO(derat): Ask germ to launch |spec_|.
  NOTIMPLEMENTED();
}

void Container::OnServiceProxyChange(ServiceInterface* service) {
  CHECK(services_.count(service->GetName()))
      << "Container \"" << GetName() << "\" received proxy change notification "
      << "for unexpected service \"" << service->GetName() << "\"";

  if (!service->GetProxy()) {
    LOG(INFO) << "Proxy for service \"" << service->GetName() << "\" within "
              << "\"" << GetName() << "\" died; relaunching container";
    Launch();
  }
}

}  // namespace psyche
