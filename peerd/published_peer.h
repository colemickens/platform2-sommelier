// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include <base/memory/weak_ptr.h>
#include <brillo/any.h>
#include <brillo/errors/error.h>

#include "peerd/peer.h"
#include "peerd/service_publisher_interface.h"

#ifndef PEERD_PUBLISHED_PEER_H_
#define PEERD_PUBLISHED_PEER_H_

namespace peerd {

class PublishedPeer : public Peer {
 public:
  using Peer::Peer;
  ~PublishedPeer() override = default;
  // Add a local service to publish over DBus.
  virtual bool AddPublishedService(
      brillo::ErrorPtr* error,
      const std::string& service_id,
      const std::map<std::string, std::string>& service_info,
      const std::map<std::string, brillo::Any>& options);
  // Overloaded to add service publishing functionality.
  bool RemoveService(brillo::ErrorPtr* error,
                     const std::string& service_id) override;

  // PublishedPeer objects will notify ServicePublishers when services are
  // added, updated, and removed.  If a publisher is added while this peer has
  // existing services, this will trigger immediate advertisement of services
  // on that publisher.
  //
  // The PublishedPeer will remove publishers implicitly when each publisher is
  // destroyed.
  virtual void RegisterServicePublisher(
      base::WeakPtr<ServicePublisherInterface> publisher);

  // Updates an existing service by setting |service_info| and |options| to
  // these new values.
  virtual bool UpdateService(
      brillo::ErrorPtr* error,
      const std::string& service_id,
      const std::map<std::string, std::string>& service_info,
      const std::map<std::string, brillo::Any>& options);

 private:
  // Removes invalid publishers.
  void CleanPublishers();
  // Notify all the publishers we know about that we have a new or updated
  // service.
  bool PublishService(brillo::ErrorPtr* error, const Service& service);

  std::vector<base::WeakPtr<ServicePublisherInterface>> publishers_;

  FRIEND_TEST(PublishedPeerTest, ShouldRejectInvalidUpdates);
  FRIEND_TEST(PublishedPeerTest, ShouldAcceptValidUpdates);
  DISALLOW_COPY_AND_ASSIGN(PublishedPeer);
};

}  // namespace peerd

#endif  // PEERD_PUBLISHED_PEER_H_
