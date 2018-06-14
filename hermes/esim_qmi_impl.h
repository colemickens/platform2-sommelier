// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_ESIM_QMI_IMPL_H_
#define HERMES_ESIM_QMI_IMPL_H_

#include <vector>

#include "hermes/esim.h"
#include "hermes/qmi_constants.h"

namespace hermes {

class EsimQmiImpl : public Esim {
 public:
  EsimQmiImpl();
  ~EsimQmiImpl() override = default;
  void GetInfo(int which,
               const DataCallback& data_callback,
               const ErrorCallback& error_callback) override;
  void GetChallenge(const DataCallback& data_callback,
                    const ErrorCallback& error_callback) override;
  void AuthenticateServer(const std::vector<uint8_t>& server_data,
                          const DataCallback& data_callback,
                          const ErrorCallback& error_callback) override;

 protected:
  void OpenChannel(uint8_t slot,
                   const DataCallback& callback,
                   const ErrorCallback& error_callback) override;
  void CloseChannel() override;
  void SendEsimMessage(const QmiCommand command,
                       const DataCallback& callback,
                       const ErrorCallback& error_callback) const;

  bool connected_;
  base::WeakPtrFactory<Esim> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(EsimQmiImpl);
};

}  // namespace hermes

#endif  // HERMES_ESIM_QMI_IMPL_H_
