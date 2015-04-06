// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_SERVICE_H_
#define PSYCHE_PSYCHED_SERVICE_H_

#include <memory>
#include <set>
#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/observer_list.h>

namespace protobinder {
class BinderProxy;
}  // namespace protobinder

namespace psyche {

class ClientInterface;
class ServiceObserver;

// A service that can be returned by psyched.
//
// ServiceInterface objects' lifetimes differ from those of the binder proxies
// that are actually returned to clients. The object is created when the service
// is first known to psyched (i.e. when the container that will provide it is
// created). Later, the process actually providing the service registers itself
// with psyched, at which point its proxy can be passed to clients. If the proxy
// dies and the service's container must be restarted, this object will be
// retained and reused once the service has been registered again.
class ServiceInterface {
 public:
  enum class State {
    STOPPED,
    STARTED,
  };

  virtual ~ServiceInterface() = default;

  virtual const std::string& GetName() const = 0;
  virtual State GetState() const = 0;
  virtual protobinder::BinderProxy* GetProxy() const = 0;

  // Updates the proxy used by clients to communicate with the service.
  virtual void SetProxy(std::unique_ptr<protobinder::BinderProxy> proxy) = 0;

  // Registers or unregisters a client as a user of this service. Ownership of
  // |client| remains with the caller.
  virtual void AddClient(ClientInterface* client) = 0;
  virtual void RemoveClient(ClientInterface* client) = 0;
  virtual bool HasClient(ClientInterface* client) const = 0;

  // Adds or removes observers of changes to this object.
  virtual void AddObserver(ServiceObserver* observer) = 0;
  virtual void RemoveObserver(ServiceObserver* observer) = 0;
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
  void SetProxy(std::unique_ptr<protobinder::BinderProxy> proxy) override;
  void AddClient(ClientInterface* client) override;
  void RemoveClient(ClientInterface* client) override;
  bool HasClient(ClientInterface* client) const override;
  void AddObserver(ServiceObserver* observer) override;
  void RemoveObserver(ServiceObserver* observer) override;

 private:
  // Invoked when |proxy_| has been closed, likely indicating that the process
  // providing the service has exited.
  void HandleBinderDeath();

  // The name of the service.
  std::string name_;

  // The service's current state.
  State state_;

  // The connection to the service that will be passed to clients.
  std::unique_ptr<protobinder::BinderProxy> proxy_;

  ObserverList<ServiceObserver> observers_;

  // Clients that are holding connections to this service.
  using ClientSet = std::set<ClientInterface*>;
  ClientSet clients_;

  // Keep this member last.
  base::WeakPtrFactory<Service> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_SERVICE_H_
