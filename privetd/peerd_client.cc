// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/peerd_client.h"

#include <map>

#include <base/message_loop/message_loop.h>
#include <chromeos/errors/error.h>
#include <chromeos/strings/string_utils.h>

#include "privetd/cloud_delegate.h"
#include "privetd/device_delegate.h"
#include "privetd/wifi_bootstrap_manager.h"
#include "privetd/wifi_ssid_generator.h"

using chromeos::string_utils::Join;
using org::chromium::peerd::PeerProxy;

namespace privetd {

namespace {

// Commit changes only if no update request happened during the timeout.
// Usually updates happen in batches, so we don't want to flood network with
// updates relevant for a short amount of time.
const int kCommitTimeoutSeconds = 1;

// The name of the service we'll expose via peerd.
const char kPrivetServiceId[] = "privet";
const char kSelfPath[] = "/org/chromium/peerd/Self";

void OnError(const std::string& operation, chromeos::Error* error) {
  LOG(ERROR) << operation << " failed:" << error->GetMessage();
}

}  // namespace

PeerdClient::PeerdClient(const scoped_refptr<dbus::Bus>& bus,
                         const DeviceDelegate* device,
                         const CloudDelegate* cloud,
                         const WifiDelegate* wifi)
    : peerd_object_manager_proxy_{bus},
      device_{device},
      cloud_{cloud},
      wifi_{wifi} {
  CHECK(device_);
  peerd_object_manager_proxy_.SetManagerAddedCallback(
      base::Bind(&PeerdClient::OnPeerdOnline, weak_ptr_factory_.GetWeakPtr()));
  peerd_object_manager_proxy_.SetManagerRemovedCallback(
      base::Bind(&PeerdClient::OnPeerdOffline, weak_ptr_factory_.GetWeakPtr()));
  peerd_object_manager_proxy_.SetPeerAddedCallback(
      base::Bind(&PeerdClient::OnNewPeer, weak_ptr_factory_.GetWeakPtr()));
}

PeerdClient::~PeerdClient() {
  RemoveService();
}

std::string PeerdClient::GetId() const {
  return device_id_;
}

void PeerdClient::Update() {
  // Abort pending updates, and wait for more changes.
  restart_weak_ptr_factory_.InvalidateWeakPtrs();
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&PeerdClient::UpdateImpl,
                            restart_weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kCommitTimeoutSeconds));
}

void PeerdClient::OnNewPeer(PeerProxy* peer) {
  if (!peer || peer->GetObjectPath().value() != kSelfPath)
    return;
  peer->SetPropertyChangedCallback(
      base::Bind(&PeerdClient::OnPeerPropertyChanged,
                 weak_ptr_factory_.GetWeakPtr()));
  OnPeerPropertyChanged(peer, PeerProxy::UUIDName());
}

void PeerdClient::OnPeerPropertyChanged(
    PeerProxy* peer,
    const std::string& property_name) {
  if (property_name != PeerProxy::UUIDName() ||
      peer->GetObjectPath().value() != kSelfPath)
    return;
  const std::string new_id{peer->uuid()};
  if (new_id != device_id_) {
    device_id_ = new_id;
    Update();
  }
}

void PeerdClient::OnPeerdOnline(
    org::chromium::peerd::ManagerProxy* manager_proxy) {
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
  const uint16_t port = device_->GetHttpEnpoint().first;
  std::map<std::string, chromeos::Any> mdns_options{
      {"port", chromeos::Any{port}},
  };

  DCHECK_NE(port, 0);
  DCHECK(!device_->GetName().empty());
  DCHECK_EQ(device_->GetClass().size(), 2U);
  DCHECK_EQ(device_->GetModelId().size(), 3U);

  std::string services;
  if (!device_->GetServices().empty())
    services += "_";
  services += Join(",_", device_->GetServices());

  std::map<std::string, std::string> txt_record{
      {"txtvers", "3"},
      {"ty", device_->GetName()},
      {"services", services},
      {"id", GetId()},
      {"class", device_->GetClass()},
      {"model_id", device_->GetModelId()},
      {"flags", WifiSsidGenerator{device_, cloud_, wifi_}.GenerateFlags()},
  };

  if (cloud_ && !cloud_->GetCloudId().empty())
    txt_record.emplace("gcd_id", cloud_->GetCloudId());

  if (!device_->GetDescription().empty())
    txt_record.emplace("note", device_->GetDescription());

  peerd_manager_proxy_->ExposeServiceAsync(
      kPrivetServiceId, txt_record, {{"mdns", mdns_options}}, base::Closure(),
      base::Bind(&OnError, "ExposeService"));
}

void PeerdClient::RemoveService() {
  if (peerd_manager_proxy_ == nullptr)
    return;

  VLOG(1) << "Stopping peerd advertising.";
  peerd_manager_proxy_->RemoveExposedServiceAsync(
      kPrivetServiceId, base::Closure(), base::Bind(&OnError, "RemoveService"));
}

void PeerdClient::UpdateImpl() {
  if (device_->GetHttpEnpoint().first == 0)
    return RemoveService();
  ExposeService();
}

}  // namespace privetd
