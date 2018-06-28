// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_ESIM_QMI_IMPL_H_
#define HERMES_ESIM_QMI_IMPL_H_

#include <libqrtr.h>

#include <base/files/scoped_file.h>

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
  void AuthenticateServer(const DataBlob& server_data,
                          const DataCallback& data_callback,
                          const ErrorCallback& error_callback) override;

  void OpenChannel(const uint8_t slot,
                   const DataCallback& data_callback,
                   const ErrorCallback& error_callback) override;
  void OnOpenChannel(const DataBlob& return_data) override;
  void CloseChannel() override;
  void SendEsimMessage(const QmiCommand command,
                       const DataCallback& callback,
                       const ErrorCallback& error_callback) const;

  // ScopedFD to hold the qrtr socket file descriptor returned by qrtr_open.
  base::ScopedFD qrtr_socket_fd_;
  base::WeakPtrFactory<EsimQmiImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(EsimQmiImpl);
};

}  // namespace hermes

#endif  // HERMES_ESIM_QMI_IMPL_H_
