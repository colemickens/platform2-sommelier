// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/privet/peerd_client.h"

#include <map>

#include <base/message_loop/message_loop.h>
#include <chromeos/errors/error.h>
#include <chromeos/strings/string_utils.h>

using org::chromium::peerd::PeerProxy;

namespace weave {
namespace privet {

namespace {

// Commit changes only if no update request happened during the timeout.
// Usually updates happen in batches, so we don't want to flood network with
// updates relevant for a short amount of time.
const int kCommitTimeoutSeconds = 1;

const char kSelfPath[] = "/org/chromium/peerd/Self";

void OnError(const std::string& operation, chromeos::Error* error) {
  LOG(ERROR) << operation << " failed:" << error->GetMessage();
}

}  // namespace

PeerdClient::PeerdClient(const scoped_refptr<dbus::Bus>& bus)
    : peerd_object_manager_proxy_{bus} {
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

void PeerdClient::PublishService(
    const std::string& service_name,
    uint16_t port,
    const std::map<std::string, std::string>& txt) {
  // Only one service supported.
  CHECK(service_name_.empty() || service_name_ == service_name);
  service_name_ = service_name;
  port_ = port;
  txt_ = txt;

  Update();
}

void PeerdClient::StopPublishing(const std::string& service_name) {
  // Only one service supported.
  CHECK(service_name_.empty() || service_name_ == service_name);
  service_name_ = service_name;
  port_ = 0;

  Update();
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
  peer->SetPropertyChangedCallback(base::Bind(
      &PeerdClient::OnPeerPropertyChanged, weak_ptr_factory_.GetWeakPtr()));
  OnPeerPropertyChanged(peer, PeerProxy::UUIDName());
}

void PeerdClient::OnPeerPropertyChanged(PeerProxy* peer,
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
  CHECK_NE(port_, 0);
  CHECK(!service_name_.empty());
  CHECK(!txt_.empty());
  std::map<std::string, chromeos::Any> mdns_options{
      {"port", chromeos::Any{port_}},
  };

  peerd_manager_proxy_->ExposeServiceAsync(
      service_name_, txt_, {{"mdns", mdns_options}}, base::Closure(),
      base::Bind(&OnError, "ExposeService"));
}

void PeerdClient::RemoveService() {
  if (peerd_manager_proxy_ == nullptr)
    return;

  VLOG(1) << "Stopping peerd advertising.";
  if (!service_name_.empty()) {
    peerd_manager_proxy_->RemoveExposedServiceAsync(
        service_name_, base::Closure(), base::Bind(&OnError, "RemoveService"));
  }
}

void PeerdClient::UpdateImpl() {
  if (port_ == 0)
    return RemoveService();
  ExposeService();
}

}  // namespace privet
}  // namespace weave
