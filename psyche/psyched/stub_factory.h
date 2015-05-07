// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_STUB_FACTORY_H_
#define PSYCHE_PSYCHED_STUB_FACTORY_H_

#include "psyche/psyched/factory_interface.h"

#include <base/macros.h>

#include <cstdint>
#include <map>
#include <string>

namespace psyche {

class CellStub;
class ClientStub;
class ServiceStub;

// Implementation of FactoryInterface that just returns stub objects. Used for
// testing.
class StubFactory : public FactoryInterface {
 public:
  StubFactory();
  ~StubFactory() override;

  // Returns the last-created stub for the given identifier.
  CellStub* GetCell(const std::string& cell_name);
  ServiceStub* GetService(const std::string& service_name);
  ClientStub* GetClient(uint32_t client_proxy_handle);

  // Sets the cell that will be returned for a CreateCell() call for a CellSpec
  // named |cell_name|. If CreateCell() is called for a cell not present here, a
  // new stub will be created automatically.
  void SetCell(const std::string& cell_name, std::unique_ptr<CellStub> cell);

  // FactoryInterface:
  std::unique_ptr<CellInterface> CreateCell(
      const soma::ContainerSpec& spec) override;
  std::unique_ptr<ServiceInterface> CreateService(
      const std::string& name) override;
  std::unique_ptr<ClientInterface> CreateClient(
      std::unique_ptr<protobinder::BinderProxy> client_proxy) override;

 private:
  // Cells, services, and clients that have been returned by Create*(). Keyed by
  // cell name, service name, and client proxy handle, respectively. Objects are
  // owned by Registrar.
  std::map<std::string, CellStub*> cells_;
  std::map<std::string, ServiceStub*> services_;
  std::map<uint32_t, ClientStub*> clients_;

  // Preset cells to return in response to CreateCell() calls, keyed by cell
  // name.
  std::map<std::string, std::unique_ptr<CellStub>> new_cells_;

  DISALLOW_COPY_AND_ASSIGN(StubFactory);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_STUB_FACTORY_H_
