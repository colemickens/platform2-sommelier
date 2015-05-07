// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_GERM_CONNECTION_H_
#define PSYCHE_PSYCHED_GERM_CONNECTION_H_

#include <string>

#include <base/macros.h>

#include "psyche/proto_bindings/soma_container_spec.pb.h"
#include "psyche/psyched/service.h"
#include "psyche/psyched/service_observer.h"

namespace germ {
class IGerm;
}  // namespace germ

namespace psyche {

// Used to communicate with germd to launch and terminate cells.
class GermConnection : public ServiceObserver {
 public:
  enum class Result {
    // The request was successful.
    SUCCESS,
    // psyched doesn't have an active binder connection to germd.
    NO_CONNECTION,
    // The request resulted in a binder-level error.
    RPC_ERROR,
    // germd responded with a failure status.
    FAILED_REQUEST,
  };

  // Returns a human-readable translation of |result|.
  static const char* ResultToString(Result result);

  GermConnection();
  ~GermConnection() override;

  // Sets the proxy that should be used for communication with germd.
  void SetProxy(std::unique_ptr<protobinder::BinderProxy> proxy);

  // Makes a request to germ to launch a cell.
  Result Launch(const soma::ContainerSpec& spec);

  // Makes a request to germ to terminate a cell with the given name.
  Result Terminate(const std::string& name);

  // ServiceObserver:
  void OnServiceProxyChange(ServiceInterface* service) override;

 private:
  Service service_;
  std::unique_ptr<germ::IGerm> interface_;

  DISALLOW_COPY_AND_ASSIGN(GermConnection);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_GERM_CONNECTION_H_
