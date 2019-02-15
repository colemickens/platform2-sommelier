// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ethernet/ethernet_provider.h"

#include <string>

#include "shill/ethernet/ethernet_service.h"
#include "shill/ethernet/ethernet_temporary_service.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/store_interface.h"

using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kEthernet;
static string ObjectID(EthernetProvider* e) {
  return "(ethernet_provider)";
}
}  // namespace Logging

EthernetProvider::EthernetProvider(ControlInterface* control_interface,
                                   EventDispatcher* dispatcher,
                                   Metrics* metrics,
                                   Manager* manager)
    : control_interface_(control_interface),
      dispatcher_(dispatcher),
      metrics_(metrics),
      manager_(manager) {}

EthernetProvider::~EthernetProvider() {}

void EthernetProvider::CreateServicesFromProfile(const ProfileRefPtr& profile) {
  SLOG(this, 2) << __func__;
  // Since the EthernetProvider's service is created during Start(),
  // there is no need to do anything in this method.
}

ServiceRefPtr EthernetProvider::FindSimilarService(const KeyValueStore& args,
                                                   Error* error) const {
  CHECK_EQ(kTypeEthernet, args.LookupString(kTypeProperty, ""))
      << "Service type must be Ethernet!";
  ServiceRefPtr service = manager_->GetFirstEthernetService();
  if (service) {
    return service;
  }
  return service_;
}

ServiceRefPtr EthernetProvider::GetService(const KeyValueStore& args,
                                           Error* error) {
  SLOG(this, 2) << __func__;
  return FindSimilarService(args, error);
}

ServiceRefPtr EthernetProvider::CreateTemporaryService(
    const KeyValueStore& args, Error* error) {
  SLOG(this, 2) << __func__;
  return new EthernetTemporaryService(
      control_interface_, dispatcher_, metrics_, manager_,
      EthernetService::kDefaultEthernetDeviceIdentifier);
}

ServiceRefPtr EthernetProvider::CreateTemporaryServiceFromProfile(
    const ProfileRefPtr& profile, const std::string& entry_name, Error* error) {
  SLOG(this, 2) << __func__;
  return new EthernetTemporaryService(control_interface_, dispatcher_, metrics_,
                                      manager_, entry_name);
}

EthernetServiceRefPtr EthernetProvider::CreateService(
    base::WeakPtr<Ethernet> ethernet) {
  SLOG(this, 2) << __func__;
  return new EthernetService(control_interface_, dispatcher_, metrics_,
                             manager_, ethernet);
}

bool EthernetProvider::LoadGenericEthernetService() {
  SLOG(this, 2) << __func__;
  return manager_->ActiveProfile()->LoadService(service_);
}

void EthernetProvider::Start() {
  SLOG(this, 2) << __func__;
  // Create a generic Ethernet service with storage ID "ethernet_any". This will
  // be used to store configurations if any are pushed down from Chrome before
  // any Ethernet devices are initialized. This will also be used to persist
  // static IP configurations across Ethernet services.
  if (!service_) {
    const ProfileRefPtr profile = manager_->ActiveProfile();
    service_ =
        new EthernetService(control_interface_, dispatcher_, metrics_, manager_,
                            EthernetService::kDefaultEthernetDeviceIdentifier);
    if (!profile->ConfigureService(service_)) {
      profile->AdoptService(service_);
    }
  }
}

void EthernetProvider::Stop() {
  SLOG(this, 2) << __func__;
  if (service_) {
    manager_->ActiveProfile()->UpdateService(service_);
  }
  // Do not destroy the service, since devices may or may not have been
  // removed as the provider is stopped, and we'd like them to continue
  // to refer to the same service on restart.
}

}  // namespace shill
