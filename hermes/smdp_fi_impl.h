// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_SMDP_FI_IMPL_H_
#define HERMES_SMDP_FI_IMPL_H_

#include <vector>

#include "hermes/smdp.h"

namespace hermes {

class SmdpFiImpl : public Smdp {
 public:
  SmdpFiImpl() = default;
  ~SmdpFiImpl() = default;

  void InitiateAuthentication(
      const std::vector<uint8_t>& challenge,
      const std::vector<uint8_t>& info1,
      const DataCallback& callback,
      const ErrorCallback& error_callback) const override;

  void AuthenticateClient(const std::vector<uint8_t>& data,
                          const DataCallback& callback,
                          const ErrorCallback& error_callback) const override;

 protected:
  void OpenConnection() const override;
  void CloseConnection() const override;
  void SendServerMessage() const override;
};

}  // namespace hermes

#endif  // HERMES_SMDP_FI_IMPL_H_
