// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_ESIM_QMI_IMPL_H_
#define HERMES_ESIM_QMI_IMPL_H_

#include <libqrtr.h>

#include <memory>

#include <base/files/scoped_file.h>
#include <base/message_loop/message_loop.h>

#include "hermes/esim.h"
#include "hermes/qmi_constants.h"

namespace hermes {

class EsimQmiImpl : public Esim, public base::MessageLoopForIO::Watcher {
 public:
  static std::unique_ptr<EsimQmiImpl> Create();
  static std::unique_ptr<EsimQmiImpl> CreateForTest(base::ScopedFD* sock);

  // Sets up FileDescriptorWatcher and transaction handling. Gets node and port
  // for UIM service from QRTR. Calls |success_callback| upon successful
  // intialization, |error_callback| on error.
  //
  // Parameters
  //  success_callback  - Closure to continue execution after eSIM is
  //                      initialized
  //  error_callack     - function to handle error if initialization fails
  void Initialize(const base::Closure& success_callback,
                  const ErrorCallback& error_callback) override;

  ~EsimQmiImpl() override = default;

  void OpenLogicalChannel(const DataCallback& data_callback,
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
  EsimQmiImpl(uint8_t slot, base::ScopedFD fd);

  void CloseChannel();
  void SendEsimMessage(const QmiCommand command,
                       const DataBlob& data,
                       const DataCallback& callback,
                       const ErrorCallback& error_callback) const;
  void SendEsimMessage(const QmiCommand command,
                       const DataCallback& callback,
                       const ErrorCallback& error_callback) const;

  // base::MessageLoopForIO::Watcher overrides.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // FileDescriptorWatcher to watch the QRTR socket for reads.
  base::MessageLoopForIO::FileDescriptorWatcher watcher_;

  // The slot that the logical channel to the eSIM will be made. Initialized in
  // constructor, hardware specific.
  uint8_t slot_;

  DataBlob buffer_;

  // Node and port to pass to qrtr_sendto, returned from qrtr_new_lookup
  // response, which contains QRTR_TYPE_NEW_SERVER and the node and port number
  // to use in following qrtr_sendto calls.
  uint32_t node_;
  uint32_t port_;

  // Logical Channel that will be used to communicate with the chip, returned
  // from OPEN_LOGICAL_CHANNEL request sent once the QRTR socket has been
  // opened.
  uint8_t channel_;

  base::Closure initialize_callback_;

  // ScopedFD to hold the qrtr socket file descriptor returned by qrtr_open.
  // Initialized in Create or CreateForTest, which will be passed to the
  // constructor, ScopedFD will be destructed with EsimQmiImpl object.
  base::ScopedFD qrtr_socket_fd_;
  base::WeakPtrFactory<EsimQmiImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(EsimQmiImpl);
};

}  // namespace hermes

#endif  // HERMES_ESIM_QMI_IMPL_H_
