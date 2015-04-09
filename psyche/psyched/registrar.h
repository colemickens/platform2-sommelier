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

namespace soma {
class ContainerSpec;
}  // namespace soma

namespace psyche {

class ClientInterface;
class ContainerInterface;
class FactoryInterface;
class ServiceInterface;
class SomaConnection;

// Owns Container and Client objects and manages communication with them.
class Registrar : public IPsychedHostInterface {
 public:
  Registrar();
  ~Registrar() override;

  // Updates |factory_|. Must be called before Init().
  void SetFactoryForTesting(std::unique_ptr<FactoryInterface> factory);

  void Init();

  // IPsychedHostInterface:
  int RegisterService(RegisterServiceRequest* in,
                      RegisterServiceResponse* out) override;
  int RequestService(RequestServiceRequest* in) override;

 private:
  // Returns the object representing |service_name|. If the service isn't
  // present in |services_| and |create_container| is true, fetches its
  // ContainerSpec from soma, launches it, and adds the service to |services_|.
  ServiceInterface* GetService(const std::string& service_name,
                               bool create_container);

  // Callback invoked when the remote side of a client's binder is closed.
  void HandleClientBinderDeath(int32_t handle);

  // Initialized by Init() if not already set by SetFactoryForTesting().
  std::unique_ptr<FactoryInterface> factory_;

  // Containers that have been created by psyche, keyed by container name.
  using ContainerMap =
      std::map<std::string, std::unique_ptr<ContainerInterface>>;
  ContainerMap containers_;

  // Services that were registered via RegisterService() but that aren't listed
  // by a container that was previously started, keyed by service name.
  // TODO(derat): Remove this once everything is running in containers.
  using ServiceMap = std::map<std::string, std::unique_ptr<ServiceInterface>>;
  ServiceMap non_container_services_;

  // Bare pointers to known (but possibly not-yet-registered) services, keyed by
  // service name. The underlying ServiceInterface objects are owned either by
  // ContainerInterface objects in |containers_| or by
  // |non_container_services_|.
  using ServicePtrMap = std::map<std::string, ServiceInterface*>;
  ServicePtrMap services_;

  // Clients that have requested services, keyed by BinderProxy handle.
  using ClientMap = std::map<int32_t, std::unique_ptr<ClientInterface>>;
  ClientMap clients_;

  // Connection to somad used to look up ContainerSpecs.
  std::unique_ptr<SomaConnection> soma_;

  // This member should appear last.
  base::WeakPtrFactory<Registrar> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Registrar);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_REGISTRAR_H_
