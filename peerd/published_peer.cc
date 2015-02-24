// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/published_peer.h"

#include <functional>

using base::WeakPtr;

namespace peerd {

bool PublishedPeer::AddPublishedService(
    chromeos::ErrorPtr* error,
    const std::string& service_id,
    const std::map<std::string, std::string>& service_info,
    const std::map<std::string, chromeos::Any>& options) {
  if (!Peer::AddService(error, service_id, {}, service_info, options)) {
    return false;
  }
  CleanPublishers();
  return PublishService(error, *services_[service_id]);
}

bool PublishedPeer::RemoveService(chromeos::ErrorPtr* error,
                                  const std::string& service_id) {
  if (!Peer::RemoveService(error, service_id)) {
    // Didn't even have this service on this peer?
    return false;
  }
  CleanPublishers();
  // Notify all the publishers we know about that we have removed a service.
  bool result = true;
  for (const auto& publisher : publishers_) {
      result = publisher->OnServiceRemoved(error, service_id) && result;
  }
  return result;
}

void PublishedPeer::RegisterServicePublisher(
    WeakPtr<ServicePublisherInterface> publisher) {
  CleanPublishers();
  if (!publisher) { return; }
  for (const auto& kv : services_) {
    publisher->OnServiceUpdated(nullptr, *kv.second);
  }
  publishers_.push_back(publisher);
}

bool PublishedPeer::UpdateService(
    chromeos::ErrorPtr* error,
    const std::string& service_id,
    const std::map<std::string, std::string>& service_info,
    const std::map<std::string, chromeos::Any>& options) {
  CleanPublishers();
  auto it = services_.find(service_id);
  if (it == services_.end()) {
    chromeos::Error::AddToPrintf(
        error, FROM_HERE, kPeerdErrorDomain, errors::peer::kUnknownService,
        "Can't update service %s because it was not previously registered.",
        service_id.c_str());
    return false;
  }
  if (!it->second->Update(error, {}, service_info, options)) {
    return false;
  }
  return PublishService(error, *it->second);
}

void PublishedPeer::CleanPublishers() {
  auto first_null = std::remove_if(
      publishers_.begin(), publishers_.end(),
      std::logical_not<base::WeakPtr<ServicePublisherInterface>>());
  publishers_.erase(first_null, publishers_.end());
}

bool PublishedPeer::PublishService(chromeos::ErrorPtr* error,
                                   const Service& service) {
  bool success = true;
  for (const auto& publisher : publishers_) {
      success = publisher->OnServiceUpdated(error, service) && success;
  }
  return success;
}

}  // namespace peerd
