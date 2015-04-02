// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_SOMA_CONNECTION_H_
#define PSYCHE_PSYCHED_SOMA_CONNECTION_H_

#include <memory>
#include <string>

#include <base/macros.h>

namespace protobinder {
class BinderProxy;
}  // namespace protobinder

namespace soma {
class ContainerSpec;
class ISoma;
}  // namespace soma

namespace psyche {

// Used to communicate with somad to look up ContainerSpecs.
class SomaConnection {
 public:
  enum Result {
    RESULT_SUCCESS,
    RESULT_RPC_ERROR,
    RESULT_UNKNOWN_SERVICE,
  };

  // Returns a human-readable translation of |result|.
  static const char* ResultToString(Result result);

  explicit SomaConnection(std::unique_ptr<protobinder::BinderProxy> proxy);
  ~SomaConnection();

  // Synchronously fetches the ContainerSpec supplying |service_name| and copies
  // it to |spec_out|.
  Result GetContainerSpecForService(const std::string& service_name,
                                    soma::ContainerSpec* spec_out);

 private:
  // TODO(derat): Instantiate a Service object for this instead.
  std::unique_ptr<protobinder::BinderProxy> proxy_;
  std::unique_ptr<soma::ISoma> interface_;

  DISALLOW_COPY_AND_ASSIGN(SomaConnection);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_SOMA_CONNECTION_H_
