// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_ESIM_QMI_IMPL_H_
#define HERMES_ESIM_QMI_IMPL_H_

#include <libqrtr.h>

#include <map>
#include <memory>
#include <utility>

#include <base/files/scoped_file.h>
#include <base/message_loop/message_loop.h>

#include "hermes/esim.h"
#include "hermes/qmi_constants.h"
#include "hermes/qmi_uim.h"

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
  void SendApdu(const DataBlob& data_buffer,
                const DataCallback& callback,
                const ErrorCallback& error_callback);

  // Messages sent between the Lpd and the eSIM chip are done through a series
  // of transactions. The Lpd can intiate transactions and the eSIM will
  // respond with a matching transaction number to the Lpd's request.
  //
  // This pair of functions map a set of callbacks data and error to the current
  // value of |current_transaction_|. The eSIM will respond after some amount of
  // time and the Lpd will read the bytes from the socket opened to the QRTR
  // socket. Finalize transaction will handle the reception of APDUs and map
  // them back to transaction numbers by doing lookups into
  // |response_callbacks_|.
  //
  // Parameters
  //  packet          - Encoded QRTR packet to send via qrtr_sendto.
  //  data_callback   - Callback to call once corresponding transaction has been
  //                    received and processed.
  //  error_callback  - Error handler after transaction has been received but
  //                    encounters an error during processing.
  void InitiateTransaction(const qrtr_packet& packet,
                           const DataCallback& data_callback,
                           const ErrorCallback& error_callback);
  // Parameters
  //  packet          - QRTR packet to decode and process.
  void FinalizeTransaction(const qrtr_packet& packet);

  uint16_t GetTransactionNumber(const qrtr_packet& packet) const;

  bool ResponseSuccess(uint16_t response) const;

  bool MorePayloadIncoming() const;

  uint8_t GetNextPayloadSize() const;

  // base::MessageLoopForIO::Watcher overrides.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // FileDescriptorWatcher to watch the QRTR socket for reads.
  base::MessageLoopForIO::FileDescriptorWatcher watcher_;

  // Counter for each initiated transaction.
  uint16_t current_transaction_;

  struct TransactionCallback {
    TransactionCallback() = default;
    TransactionCallback(const DataCallback& data_callback,
                        const ErrorCallback& error_callback);
    DataCallback data_callback;
    ErrorCallback error_callback;
  };

  // Mapping of transactions to callbacks from Lpd once the eSIM has responded
  // to a request.
  std::map<int, TransactionCallback> response_callbacks_;

  // The slot that the logical channel to the eSIM will be made. Initialized in
  // constructor, hardware specific.
  uint8_t slot_;

  // Buffer for storing data from the QRTR socket.
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

  // Status returned as the last two bytes in APDU messages
  uint8_t sw1_;
  uint8_t sw2_;

  // All APDU bytes. sw1_ and sw2_ are extracted immediately after reception.
  DataBlob payload_;

  // Closure given to Esim::Initialize to call once the UIM service has been
  // exposed through QRTR and |node_| and |port_| have been set.
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
