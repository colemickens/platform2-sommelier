// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/esim_uim_impl.h"

#include <vector>

namespace hermes {

void EsimUimImpl::GetInfo(int which,
                          const DataCallback& callback,
                          const ErrorCallback& error_callback) {
  const std::vector<uint8_t> test_info = {1, 2, 3, 4, 5};
  callback.Run(test_info);
}

void EsimUimImpl::GetChallenge(const DataCallback& callback,
                               const ErrorCallback& error_callback) {
  const std::vector<uint8_t> test_challenge = {0x10, 0x11, 0x12,
                                               0x13, 0x14, 0x15};
  callback.Run(test_challenge);
}

void EsimUimImpl::AuthenticateServer(const std::vector<uint8_t>& server_data,
                                     const DataCallback& callback,
                                     const ErrorCallback& error_callback) {
  std::vector<uint8_t> test_esim_response = {0x20, 0x21, 0x22, 0x23, 0x24};
  callback.Run(test_esim_response);
}

void EsimUimImpl::OpenChannel() const {}
void EsimUimImpl::CloseChannel() const {}
void EsimUimImpl::SendEsimMessage() const {}

}  // namespace hermes
