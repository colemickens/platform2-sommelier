// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/peerd_client.h"

#include <map>
#include <utility>
#include <vector>

#include <base/message_loop/message_loop.h>
#include <brillo/errors/error.h>
#include <brillo/strings/string_utils.h>

namespace buffet {

namespace {

// Commit changes only if no update request happened during the timeout.
// Usually updates happen in batches, so we don't want to flood network with
// updates relevant for a short amount of time.
const int kCommitTimeoutSeconds = 1;

void OnError(const std::string& operation, brillo::Error* error) {
  LOG(ERROR) << operation << " failed:" << error->GetMessage();
}

const char kExpectedServiceType[] = "_privet._tcp";
const char kServiceName[] = "privet";

}  // namespace

PeerdClient::PeerdClient(const scoped_refptr<dbus::Bus>& bus)
    : peerd_object_manager_proxy_{bus} {
  peerd_object_manager_proxy_.SetManagerAddedCallback(
      base::Bind(&PeerdClient::OnPeerdOnline, weak_ptr_factory_.GetWeakPtr()));
  peerd_object_manager_proxy_.SetManagerRemovedCallback(
      base::Bind(&PeerdClient::OnPeerdOffline, weak_ptr_factory_.GetWeakPtr()));
}

PeerdClient::~PeerdClient() {
  RemoveService();
}

void PeerdClient::PublishService(const std::string& service_type,
                                 uint16_t port,
                                 const std::vector<std::string>& txt) {
  // Only one service supported.
  CHECK_EQ(service_type, kExpectedServiceType);
  port_ = port;
  txt_ = txt;
  Update();
}

void PeerdClient::StopPublishing(const std::string& service_type) {
  // Only one service supported.
  CHECK_EQ(service_type, kExpectedServiceType);
  port_ = 0;
  Update();
}

void PeerdClient::Update() {
  // Abort pending updates, and wait for more changes.
  restart_weak_ptr_factory_.InvalidateWeakPtrs();
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PeerdClient::UpdateImpl,
                 restart_weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kCommitTimeoutSeconds));
}

void PeerdClient::OnPeerdOnline(
    org::chromium::peerd::ManagerProxyInterface* manager_proxy) {
  peerd_manager_proxy_ = manager_proxy;
  VLOG(1) << "Peerd manager is online at '"
          << manager_proxy->GetObjectPath().value() << "'.";
  Update();
}

void PeerdClient::OnPeerdOffline(const dbus::ObjectPath& object_path) {
  peerd_manager_proxy_ = nullptr;
  VLOG(1) << "Peerd manager is now offline.";
}

void PeerdClient::ExposeService() {
  // Do nothing if peerd hasn't started yet.
  if (peerd_manager_proxy_ == nullptr)
    return;
  VLOG(1) << "Starting peerd advertising.";
  CHECK_NE(port_, 0);
  CHECK(!txt_.empty());
  std::map<std::string, brillo::Any> mdns_options{
      {"port", brillo::Any{port_}},
  };

  std::map<std::string, std::string> txt;
  for (const auto& record : txt_) {
    auto name_value = brillo::string_utils::SplitAtFirst(record, "=");
    CHECK(!name_value.second.empty());
    txt.emplace(std::move(name_value));
  }

  published_ = true;
  peerd_manager_proxy_->ExposeServiceAsync(
      kServiceName, txt, {{"mdns", mdns_options}}, base::Closure(),
      base::Bind(&OnError, "ExposeService"));
}

void PeerdClient::RemoveService() {
  if (peerd_manager_proxy_ == nullptr)
    return;

  VLOG(1) << "Stopping peerd advertising.";
  if (published_) {
    published_ = false;
    peerd_manager_proxy_->RemoveExposedServiceAsync(
        kServiceName, base::Closure(), base::Bind(&OnError, "RemoveService"));
  }
}

void PeerdClient::UpdateImpl() {
  if (port_ == 0)
    return RemoveService();
  ExposeService();
}

}  // namespace buffet
