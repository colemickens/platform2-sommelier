// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_REGISTRAR_H_
#define PSYCHE_PSYCHED_REGISTRAR_H_

#include <map>
#include <string>

#include <base/macros.h>
#include <base/memory/linked_ptr.h>
#include <base/memory/weak_ptr.h>

#include "psyche/proto_bindings/psyche.pb.h"
#include "psyche/proto_bindings/psyche.pb.rpc.h"

namespace protobinder {
class BinderProxy;
}  // namespace protobinder

namespace psyche {

class Client;
class Service;

// Holds Service and Client objects and manages communication with them.
class Registrar : public IPsychedHostInterface {
 public:
  Registrar();
  ~Registrar() override;

  // IPsychedHostInterface:
  int RegisterService(RegisterServiceRequest* in,
                      RegisterServiceResponse* out) override;
  int RequestService(RequestServiceRequest* in,
                     RequestServiceResponse* out) override;

 private:
  // Callback invoked when the remote side of a client's binder is closed.
  void HandleClientBinderDeath(int32_t handle);

  // Keyed by service name.
  using ServiceMap = std::map<std::string, linked_ptr<Service>>;
  ServiceMap services_;

  // Keyed by BinderProxy handle.
  using ClientMap = std::map<int32_t, linked_ptr<Client>>;
  ClientMap clients_;

  // This member should appear last.
  base::WeakPtrFactory<Registrar> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Registrar);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_REGISTRAR_H_
