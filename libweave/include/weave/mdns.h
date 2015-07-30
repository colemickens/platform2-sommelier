// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_MDNS_H_
#define LIBWEAVE_INCLUDE_WEAVE_MDNS_H_

#include <map>
#include <string>

#include <base/callback.h>

namespace weave {

class Mdns {
 public:
  // Publishes new service on mDns or updates existing one.
  virtual void PublishService(
      const std::string& service_name,
      uint16_t port,
      const std::map<std::string, std::string>& txt) = 0;

  // Stops publishing service.
  virtual void StopPublishing(const std::string& service_name) = 0;

  // Returns permanent device ID.
  // TODO(vitalybuka): Find better place for this information.
  virtual std::string GetId() const = 0;

 protected:
  virtual ~Mdns() = default;
};

}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_MDNS_H_
