// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ETHERNET_EAP_PROVIDER_H_
#define SHILL_ETHERNET_EAP_PROVIDER_H_

#include <map>

#include <base/callback.h>

#include "shill/refptr_types.h"

namespace shill {

class ControlInterface;
class Ethernet;
class EventDispatcher;
class Manager;
class Metrics;

class EthernetEapProvider {
 public:
  typedef base::Callback<void()> CredentialChangeCallback;

  EthernetEapProvider(ControlInterface *control_interface,
                      EventDispatcher *dispatcher,
                      Metrics *metrics,
                      Manager *manager);
  virtual ~EthernetEapProvider();

  virtual void Start();
  virtual void Stop();

  virtual const ServiceRefPtr &service() const { return service_; }

  // Notify |device| via |callback| when EAP credentials have changed.
  // Any previous callbacks for |device| are removed.  |device| is
  // only used a key to a map and is never dereferenced.
  virtual void SetCredentialChangeCallback(Ethernet *device,
                                           CredentialChangeCallback callback);

  // Clear any previously registered callback for |device|.
  virtual void ClearCredentialChangeCallback(Ethernet *device);

  // Called by |service_| when EAP credentials are changed.  Notify all
  // listening Ethernet devices.
  virtual void OnCredentialsChanged() const;

 private:
  friend class EthernetEapProviderTest;
  friend class EthernetTest;
  friend class ManagerTest;

  // Used only in Ethernet and Manager unit tests.
  // TODO(gauravsh): Remove this and allow mocks to work correctly
  // crbug.com/232134
  void set_service(const ServiceRefPtr &service) { service_ = service; }

  typedef std::map<Ethernet *, CredentialChangeCallback> CallbackMap;

  // Representative service on which EAP credentials are configured.
  ServiceRefPtr service_;

  // Keyed set of notifiers to call when the EAP credentials for |service_|
  // have changed.
  CallbackMap callback_map_;

  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Metrics *metrics_;
  Manager *manager_;
};

}  // namespace shill

#endif  // SHILL_ETHERNET_EAP_PROVIDER_H_
