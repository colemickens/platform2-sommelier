// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_CELL_H_
#define PSYCHE_PSYCHED_CELL_H_

#include <map>
#include <memory>
#include <string>

#include <base/macros.h>

#include "psyche/proto_bindings/soma_container_spec.pb.h"
#include "psyche/psyched/service_observer.h"

namespace psyche {

class FactoryInterface;
class GermConnection;
class ServiceInterface;

// Corresponds to a ContainerSpec returned by soma and launched one or more
// times by germ. This class persists across multiple launches of the cell.
class CellInterface {
 public:
  // Keyed by service name.
  using ServiceMap = std::map<std::string, std::unique_ptr<ServiceInterface>>;

  virtual ~CellInterface() = default;

  // Returns this cell's name.
  virtual std::string GetName() const = 0;

  // Returns the services provided by this cell. Binder proxies for these
  // services have not necessarily been registered yet.
  virtual const ServiceMap& GetServices() const = 0;

  // Launches the cell. Returns whether the cell was launched successfully.
  virtual bool Launch() = 0;

  // Terminates the cell. Returns whether the cell was terminated successfully.
  virtual bool Terminate() = 0;
};

// The real implementation of CellInterface.
class Cell : public CellInterface, public ServiceObserver {
 public:
  // |factory| is used to construct ServiceInterface objects, permitting tests
  // to create stub services instead. Note: Ownership of |germ| remains with
  // the caller.
  Cell(const soma::ContainerSpec& spec,
       FactoryInterface* factory,
       GermConnection* germ);
  ~Cell() override;

  // CellInterface:
  std::string GetName() const override;
  const ServiceMap& GetServices() const override;
  bool Launch() override;
  bool Terminate() override;

  // ServiceObserver:
  void OnServiceProxyChange(ServiceInterface* service) override;

 private:
  // The specification describing this cell.
  soma::ContainerSpec spec_;

  // Services that are provided by this cell. These are created when the service
  // is created; the binder proxies that are given to clients are set later when
  // the services are registered.
  ServiceMap services_;

  // Connection to germd to launch the cell. Note: This class doesn't own the
  // GermConnection object.
  GermConnection* germ_connection_;

  DISALLOW_COPY_AND_ASSIGN(Cell);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_CELL_H_
