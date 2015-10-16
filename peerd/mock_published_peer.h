// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_MOCK_PUBLISHED_PEER_H_
#define PEERD_MOCK_PUBLISHED_PEER_H_

#include <map>
#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

#include <base/memory/weak_ptr.h>
#include <base/time/time.h>
#include <brillo/dbus/dbus_object.h>
#include <brillo/errors/error.h>
#include <dbus/object_path.h>
#include <gmock/gmock.h>

#include "peerd/published_peer.h"
#include "peerd/service_publisher_interface.h"

namespace peerd {

class MockPublishedPeer : public PublishedPeer {
 public:
  MockPublishedPeer(const scoped_refptr<dbus::Bus>& bus,
                    const dbus::ObjectPath& path)
      : PublishedPeer(bus, nullptr, path) { }
  ~MockPublishedPeer() override = default;

  MOCK_METHOD4(RegisterAsync,
               bool(brillo::ErrorPtr* error,
                    const std::string& uuid,
                    uint64_t last_seen,
                    const CompletionAction& completion_callback));
  MOCK_CONST_METHOD0(GetUUID, std::string());
  MOCK_METHOD2(SetLastSeen, bool(brillo::ErrorPtr* error,
                                 const base::Time& last_seen));

  MOCK_METHOD4(AddPublishedService,
               bool(brillo::ErrorPtr* error,
                    const std::string& service_id,
                    const std::map<std::string, std::string>& service_info,
                    const std::map<std::string, brillo::Any>& options));
  MOCK_METHOD2(RemoveService, bool(brillo::ErrorPtr* error,
                                   const std::string& service_id));
  MOCK_METHOD1(RegisterServicePublisher,
               void(base::WeakPtr<ServicePublisherInterface> publisher));
};

}  // namespace peerd

#endif  // PEERD_MOCK_PUBLISHED_PEER_H_
