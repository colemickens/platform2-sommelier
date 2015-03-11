// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_SERVICE_H_
#define PSYCHE_PSYCHED_SERVICE_H_

#include <set>
#include <string>

#include <base/macros.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>

namespace protobinder {
class BinderProxy;
}  // namespace protobinder

namespace psyche {

class Client;

// A service that is registered with psyched.
class Service {
 public:
  enum State {
    STATE_STOPPED = 0,
    STATE_STARTED,
  };

  explicit Service(const std::string& name);
  ~Service();

  const std::string& name() const { return name_; }
  State state() const { return state_; }
  protobinder::BinderProxy* proxy() { return proxy_.get(); }

  using ClientSet = std::set<Client*>;
  const ClientSet& clients() const { return clients_; }

  // Updates the proxy used by clients to communicate with the service.
  void SetProxy(scoped_ptr<protobinder::BinderProxy> proxy);

  // Registers or unregisters a client as a user of this service. Ownership of
  // |client| remains with the caller.
  void AddClient(Client* client);
  void RemoveClient(Client* client);

 private:
  // Lets |clients_| know that |state_| has changed.
  void NotifyClientsAboutStateChange();

  // Invoked when |proxy_| has been closed, likely indicating that the process
  // providing the service has exited.
  void HandleBinderDeath();

  // The name of the service.
  std::string name_;

  // The service's current state.
  State state_;

  // The connection to the service that will be passed to clients.
  scoped_ptr<protobinder::BinderProxy> proxy_;

  // Clients that are holding connections to this service.
  ClientSet clients_;

  // Keep this member last.
  base::WeakPtrFactory<Service> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_SERVICE_H_
