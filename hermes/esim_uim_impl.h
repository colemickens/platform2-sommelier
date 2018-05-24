// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_ESIM_UIM_IMPL_H_
#define HERMES_ESIM_UIM_IMPL_H_

#include <vector>

#include "hermes/esim.h"

namespace hermes {

class EsimUimImpl : public Esim {
 public:
  EsimUimImpl() = default;
  ~EsimUimImpl() = default;
  void GetInfo(int which,
               const DataCallback& callback,
               const ErrorCallback& error_callback) override;
  void GetChallenge(const DataCallback& callback,
                    const ErrorCallback& error_callback) override;
  void AuthenticateServer(const std::vector<uint8_t>& server_data,
                          const DataCallback& callback,
                          const ErrorCallback& error_callback) override;

 protected:
  void OpenChannel() const override;
  void CloseChannel() const override;
  void SendEsimMessage() const override;
};

}  // namespace hermes

#endif  // HERMES_ESIM_UIM_IMPL_H_
