// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/published_peer.h"

#include <functional>

using base::WeakPtr;

namespace peerd {

bool PublishedPeer::AddService(
    chromeos::ErrorPtr* error,
    const std::string& service_id,
    const std::vector<ip_addr>& addresses,
    const std::map<std::string, std::string>& service_info) {
  if (!Peer::AddService(error, service_id, addresses, service_info)) {
    return false;
  }
  bool success = true;
  // Notify all the publishers we know about that we have a new service.
  auto first_null = std::remove_if(
      publishers_.begin(), publishers_.end(),
      std::logical_not<base::WeakPtr<ServicePublisherInterface>>());
  publishers_.erase(first_null, publishers_.end());
  for (const auto& publisher : publishers_) {
      success = publisher->OnServiceUpdated(
          error, *services_[service_id]) && success;
  }
  return success;
}

bool PublishedPeer::RemoveService(chromeos::ErrorPtr* error,
                                  const std::string& service_id) {
  if (!Peer::RemoveService(error, service_id)) {
    // Didn't even have this service on this peer?
    return false;
  }
  // Notify all the publishers we know about that we have removed a service.
  bool result = true;
  auto first_null = std::remove_if(
      publishers_.begin(), publishers_.end(),
      std::logical_not<base::WeakPtr<ServicePublisherInterface>>());
  publishers_.erase(first_null, publishers_.end());
  for (const auto& publisher : publishers_) {
      result = publisher->OnServiceRemoved(error, service_id) && result;
  }
  return result;
}

void PublishedPeer::RegisterServicePublisher(
    WeakPtr<ServicePublisherInterface> publisher) {
  if (!publisher) { return; }
  for (const auto& kv : services_) {
    publisher->OnServiceUpdated(nullptr, *kv.second);
  }
  publishers_.push_back(publisher);
}

}  // namespace peerd
