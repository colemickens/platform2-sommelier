// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ETHERNET_ETHERNET_PROVIDER_H_
#define SHILL_ETHERNET_ETHERNET_PROVIDER_H_

#include <string>

#include <base/callback.h>
#include <base/memory/weak_ptr.h>

#include "shill/provider_interface.h"
#include "shill/refptr_types.h"

namespace shill {

class ControlInterface;
class Error;
class Ethernet;
class EventDispatcher;
class KeyValueStore;
class Manager;
class Metrics;

class EthernetProvider : public ProviderInterface {
 public:
  EthernetProvider(ControlInterface* control_interface,
                   EventDispatcher* dispatcher,
                   Metrics* metrics,
                   Manager* manager);
  ~EthernetProvider() override;

  // Called by Manager as a part of the Provider interface.
  void CreateServicesFromProfile(const ProfileRefPtr& profile) override;
  ServiceRefPtr GetService(const KeyValueStore& args, Error* error) override;
  ServiceRefPtr FindSimilarService(const KeyValueStore& args,
                                   Error* error) const override;
  ServiceRefPtr CreateTemporaryService(const KeyValueStore& args,
                                       Error* error) override;
  ServiceRefPtr CreateTemporaryServiceFromProfile(const ProfileRefPtr& profile,
                                                  const std::string& entry_name,
                                                  Error* error) override;
  void Start() override;
  void Stop() override;

  virtual EthernetServiceRefPtr CreateService(base::WeakPtr<Ethernet> ethernet);
  bool LoadGenericEthernetService();

  virtual const ServiceRefPtr& service() const { return service_; }

 private:
  friend class EthernetProviderTest;
  friend class ManagerTest;

  // Representative service
  ServiceRefPtr service_;

  ControlInterface* control_interface_;
  EventDispatcher* dispatcher_;
  Metrics* metrics_;
  Manager* manager_;

  DISALLOW_COPY_AND_ASSIGN(EthernetProvider);
};

}  // namespace shill

#endif  // SHILL_ETHERNET_ETHERNET_PROVIDER_H_
