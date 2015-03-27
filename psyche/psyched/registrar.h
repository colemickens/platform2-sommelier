// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_REGISTRAR_H_
#define PSYCHE_PSYCHED_REGISTRAR_H_

#include <map>
#include <memory>
#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>

#include "psyche/proto_bindings/psyche.pb.h"
#include "psyche/proto_bindings/psyche.pb.rpc.h"

namespace protobinder {
class BinderProxy;
}  // namespace protobinder

namespace psyche {

class ClientInterface;
class ServiceInterface;

// Holds Service and Client objects and manages communication with them.
class Registrar : public IPsychedHostInterface {
 public:
  // Creates objects on behalf of registrar. Defined as an interface so unit
  // tests can return stub objects.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual std::unique_ptr<ServiceInterface> CreateService(
        const std::string& name) = 0;
    virtual std::unique_ptr<ClientInterface> CreateClient(
        std::unique_ptr<BinderProxy> client_proxy) = 0;
  };

  Registrar();
  ~Registrar() override;

  // Updates |delegate_|. Must be called before Init().
  void SetDelegateForTesting(std::unique_ptr<Delegate> delegate);

  void Init();

  // IPsychedHostInterface:
  int RegisterService(RegisterServiceRequest* in,
                      RegisterServiceResponse* out) override;
  int RequestService(RequestServiceRequest* in,
                     RequestServiceResponse* out) override;

 private:
  class RealDelegate;

  // Callback invoked when the remote side of a client's binder is closed.
  void HandleClientBinderDeath(int32_t handle);

  // Initialized by Init() if not already set by SetDelegateForTesting().
  std::unique_ptr<Delegate> delegate_;

  // Keyed by service name.
  using ServiceMap = std::map<std::string, std::unique_ptr<ServiceInterface>>;
  ServiceMap services_;

  // Keyed by BinderProxy handle.
  using ClientMap = std::map<int32_t, std::unique_ptr<ClientInterface>>;
  ClientMap clients_;

  // This member should appear last.
  base::WeakPtrFactory<Registrar> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Registrar);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_REGISTRAR_H_
