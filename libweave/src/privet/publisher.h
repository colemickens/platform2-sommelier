// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_PRIVET_PUBLISHER_H_
#define LIBWEAVE_SRC_PRIVET_PUBLISHER_H_

#include <memory>
#include <string>

#include <base/macros.h>

#include "libweave/src/privet/identity_delegate.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace weave {

class Mdns;

namespace privet {

class CloudDelegate;
class DeviceDelegate;
class WifiDelegate;

// Publishes privet service on mDns.
class Publisher : public IdentityDelegate {
 public:
  Publisher(const DeviceDelegate* device,
            const CloudDelegate* cloud,
            const WifiDelegate* wifi,
            Mdns* mdns);
  ~Publisher() override;

  // IdentityDelegate implementation.
  std::string GetId() const override;

  // Updates published information.  Removes service if HTTP is not alive.
  void Update();

 private:
  void ExposeService();
  void RemoveService();

  Mdns* mdns_{nullptr};

  const DeviceDelegate* device_{nullptr};
  const CloudDelegate* cloud_{nullptr};
  const WifiDelegate* wifi_{nullptr};

  DISALLOW_COPY_AND_ASSIGN(Publisher);
};

}  // namespace privet
}  // namespace weave

#endif  // LIBWEAVE_SRC_PRIVET_PUBLISHER_H_
