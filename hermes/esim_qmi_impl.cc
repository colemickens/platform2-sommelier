// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/esim_qmi_impl.h"

#include <vector>

namespace hermes {

EsimQmiImpl::EsimQmiImpl() : connected_(false), weak_factory_(this) {}

// TODO(jruthe): pass |which| to EsimQmiImpl::SendEsimMessage to make the
// correct libqrtr call to the eSIM chip.
void EsimQmiImpl::GetInfo(int which,
                          const DataCallback& callback,
                          const ErrorCallback& error_callback) {
  if (!connected_) {
    // error_callback.Run();
  }

  SendEsimMessage(QmiCommand::kSendApdu, callback, error_callback);
}

void EsimQmiImpl::GetChallenge(const DataCallback& callback,
                               const ErrorCallback& error_callback) {
  if (!connected_) {
    // error_callback.Run()
  }

  SendEsimMessage(QmiCommand::kSendApdu, callback, error_callback);
}

// TODO(jruthe): pass |server_data| to EsimQmiImpl::SendEsimMessage to make
// correct libqrtr call to the eSIM chip.
void EsimQmiImpl::AuthenticateServer(const std::vector<uint8_t>& server_data,
                                     const DataCallback& callback,
                                     const ErrorCallback& error_callback) {
  if (!connected_) {
    // error_callback.Run(server_data);
  }

  SendEsimMessage(QmiCommand::kSendApdu, callback, error_callback);
}

void EsimQmiImpl::OpenChannel(uint8_t slot,
                              const DataCallback& callback,
                              const ErrorCallback& error_callback) const {
  SendEsimMessage(QmiCommand::kOpenLogicalChannel, callback, error_callback);
}

void EsimQmiImpl::CloseChannel() const {}

void EsimQmiImpl::SendEsimMessage(const QmiCommand command,
                                  const DataCallback& callback,
                                  const ErrorCallback& error_callback) const {
  std::vector<uint8_t> result_code_tlv;
  switch (command) {
    case QmiCommand::kOpenLogicalChannel:
      // std::vector<uint8_t> slot_tlv = {0x01, 0x01, 0x00, 0x01};
      // TODO(jruthe): insert actual PostTask for QMI call here to open logical
      // channel and populate result_code_tlv with return data from
      // SEND_APDU_IND QMI callback
      //
      // base::ThreadTaskRunnerHandle::Get->PostTask(...)
      result_code_tlv = {0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
      callback.Run(result_code_tlv);
      break;
    case QmiCommand::kLogicalChannel:
      // TODO(jruthe) insert PostTask for closing logical channel
      result_code_tlv = {0x00};
      callback.Run(result_code_tlv);
      break;
    case QmiCommand::kSendApdu:
      // TODO(jruthe): implement some logic to construct different APDUs.

      // TODO(jruthe): insert actual PostTask for SEND_APDU QMI call
      result_code_tlv = {0x00};
      callback.Run(result_code_tlv);
      break;
  }
}

}  // namespace hermes
