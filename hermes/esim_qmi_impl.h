// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_ESIM_QMI_IMPL_H_
#define HERMES_ESIM_QMI_IMPL_H_

#include <libqrtr.h>

#include <memory>

#include <base/files/scoped_file.h>

#include "hermes/esim.h"
#include "hermes/qmi_constants.h"

namespace hermes {

class EsimQmiImpl : public Esim {
 public:
  static std::unique_ptr<EsimQmiImpl> Create();
  static std::unique_ptr<EsimQmiImpl> CreateForTest(base::ScopedFD* sock);

  ~EsimQmiImpl() override = default;

  // Open logical channel on |slot_| to the eSIM. The slot specified should be
  // the one associated with the eSIM chip.
  //
  // Parameters
  //  callback      - function to call once the channel has been successfully
  //                  opened
  //  error_callack - function handle error if eSIM fails to open channel
  void Initialize(const DataCallback& data_callback,
                  const ErrorCallback& error_callback) override;
  void GetInfo(int which,
               const DataCallback& data_callback,
               const ErrorCallback& error_callback) override;
  void GetChallenge(const DataCallback& data_callback,
                    const ErrorCallback& error_callback) override;
  void AuthenticateServer(const DataBlob& server_data,
                          const DataCallback& data_callback,
                          const ErrorCallback& error_callback) override;

 private:
  EsimQmiImpl(const uint8_t slot, base::ScopedFD fd);

  void OnOpenChannel(const DataCallback& data_callback,
                     const ErrorCallback& error_callback,
                     const DataBlob& return_data);
  void CloseChannel();
  void SendEsimMessage(const QmiCommand command,
                       const DataBlob& data,
                       const DataCallback& callback,
                       const ErrorCallback& error_callback) const;
  void SendEsimMessage(const QmiCommand command,
                       const DataCallback& callback,
                       const ErrorCallback& error_callback) const;

  // The slot that the logical channel to the eSIM will be made. Initialized in
  // constructor, hardware specific.
  uint8_t slot_;

  // ScopedFD to hold the qrtr socket file descriptor returned by qrtr_open.
  // Initialized in Create or CreateForTest, which will be passed to the
  // constructor, ScopedFD will be destructed with EsimQmiImpl object.
  base::ScopedFD qrtr_socket_fd_;
  base::WeakPtrFactory<EsimQmiImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(EsimQmiImpl);
};

}  // namespace hermes

#endif  // HERMES_ESIM_QMI_IMPL_H_
