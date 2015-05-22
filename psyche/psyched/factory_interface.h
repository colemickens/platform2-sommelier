// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_FACTORY_INTERFACE_H_
#define PSYCHE_PSYCHED_FACTORY_INTERFACE_H_

#include <memory>
#include <string>

namespace protobinder {
class BinderProxy;
}  // namespace protobinder

namespace soma {
class SandboxSpec;
}  // namespace soma

namespace psyche {

class CellInterface;
class ClientInterface;
class ServiceInterface;

// Interface to create various objects. Defined as an interface so unit tests
// can create stub objects.
class FactoryInterface {
 public:
  virtual ~FactoryInterface() = default;

  virtual std::unique_ptr<CellInterface> CreateCell(
      const soma::SandboxSpec& spec) = 0;
  virtual std::unique_ptr<ServiceInterface> CreateService(
      const std::string& name) = 0;
  virtual std::unique_ptr<ClientInterface> CreateClient(
      std::unique_ptr<protobinder::BinderProxy> client_proxy) = 0;
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_FACTORY_INTERFACE_H_
