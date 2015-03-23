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

class ClientInterface;

// A service that is registered with psyched.
class ServiceInterface {
 public:
  enum State {
    STATE_STOPPED = 0,
    STATE_STARTED,
  };

  virtual ~ServiceInterface() = default;

  virtual const std::string& GetName() const = 0;
  virtual State GetState() const = 0;
  virtual protobinder::BinderProxy* GetProxy() const = 0;

  // Updates the proxy used by clients to communicate with the service.
  virtual void SetProxy(scoped_ptr<protobinder::BinderProxy> proxy) = 0;

  // Registers or unregisters a client as a user of this service. Ownership of
  // |client| remains with the caller.
  virtual void AddClient(ClientInterface* client) = 0;
  virtual void RemoveClient(ClientInterface* client) = 0;
  virtual bool HasClient(ClientInterface* client) const = 0;
};

// Real implementation of ServiceInterface.
class Service : public ServiceInterface {
 public:
  explicit Service(const std::string& name);
  ~Service() override;

  // ServiceInterface:
  const std::string& GetName() const override;
  State GetState() const override;
  protobinder::BinderProxy* GetProxy() const override;
  void SetProxy(scoped_ptr<protobinder::BinderProxy> proxy) override;
  void AddClient(ClientInterface* client) override;
  void RemoveClient(ClientInterface* client) override;
  bool HasClient(ClientInterface* client) const override;

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
  using ClientSet = std::set<ClientInterface*>;
  ClientSet clients_;

  // Keep this member last.
  base::WeakPtrFactory<Service> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_SERVICE_H_
