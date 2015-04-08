// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_SOMA_CONNECTION_H_
#define PSYCHE_PSYCHED_SOMA_CONNECTION_H_

#include <memory>
#include <string>

#include <base/macros.h>

#include "psyche/psyched/service.h"
#include "psyche/psyched/service_observer.h"

namespace protobinder {
class BinderProxy;
}  // namespace protobinder

namespace soma {
class ContainerSpec;
class ISoma;
}  // namespace soma

namespace psyche {

// Used to communicate with somad to look up ContainerSpecs.
class SomaConnection : public ServiceObserver {
 public:
  enum class Result {
    // The request was successful.
    SUCCESS,
    // psyched doesn't have an active binder connection to somad.
    NO_SOMA_CONNECTION,
    // The request resulted in a binder-level error.
    RPC_ERROR,
    // somad doesn't know anything about the requested service.
    UNKNOWN_SERVICE,
  };

  // Returns a human-readable translation of |result|.
  static const char* ResultToString(Result result);

  SomaConnection();
  ~SomaConnection() override;

  // Sets the proxy that should be used for communication with somad.
  void SetProxy(std::unique_ptr<protobinder::BinderProxy> proxy);

  // Synchronously fetches the ContainerSpec supplying |service_name| and copies
  // it to |spec_out|.
  Result GetContainerSpecForService(const std::string& service_name,
                                    soma::ContainerSpec* spec_out);

  // ServiceObserver:
  void OnServiceProxyChange(ServiceInterface* service) override;

 private:
  Service service_;
  std::unique_ptr<soma::ISoma> interface_;

  DISALLOW_COPY_AND_ASSIGN(SomaConnection);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_SOMA_CONNECTION_H_
