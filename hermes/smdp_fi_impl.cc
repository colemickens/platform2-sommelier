// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/smdp_fi_impl.h"

namespace hermes {

void SmdpFiImpl::InitiateAuthentication(
    const std::vector<uint8_t>& challenge,
    const std::vector<uint8_t>& info1,
    const DataCallback& callback,
    const ErrorCallback& error_callback) const {
  const std::vector<uint8_t> return_data = {5, 10, 15, 20, 25};
  callback.Run(return_data);
}

void SmdpFiImpl::AuthenticateClient(const std::vector<uint8_t>& data,
                                    const DataCallback& callback,
                                    const ErrorCallback& error_callback) const {
  const std::vector<uint8_t> return_data = {2, 4, 6, 8, 10};
  callback.Run(return_data);
}

void SmdpFiImpl::OpenConnection() const {}
void SmdpFiImpl::CloseConnection() const {}
void SmdpFiImpl::SendServerMessage() const {}

}  // namespace hermes
