// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ETHERNET_SERVICE_
#define SHILL_ETHERNET_SERVICE_

#include <base/basictypes.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/event_dispatcher.h"
#include "shill/refptr_types.h"
#include "shill/service.h"

namespace shill {

class ControlInterface;
class EventDispatcher;
class Manager;
class Metrics;

class EthernetService : public Service {
 public:
  EthernetService(ControlInterface *control_interface,
                  EventDispatcher *dispatcher,
                  Metrics *metrics,
                  Manager *manager,
                  const EthernetRefPtr &device);
  ~EthernetService();

  // Inherited from Service.
  virtual void Connect(Error *error, const char *reason);
  virtual void Disconnect(Error *error);

  // ethernet_<MAC>
  virtual std::string GetStorageIdentifier() const;
  virtual bool IsAutoConnectByDefault() const { return true; }
  virtual bool SetAutoConnectFull(const bool &connect, Error *error);

  virtual void Remove(Error *error);

 protected:
  virtual std::string GetTethering(Error *error) const override;

 private:
  FRIEND_TEST(EthernetServiceTest, GetTethering);

  static const char kServiceType[];

  std::string GetDeviceRpcId(Error *error) const;

  EthernetRefPtr ethernet_;
  DISALLOW_COPY_AND_ASSIGN(EthernetService);
};

}  // namespace shill

#endif  // SHILL_ETHERNET_SERVICE_
