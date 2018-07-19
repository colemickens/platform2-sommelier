// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/esim_qmi_impl.h"

#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/memory/ptr_util.h>

#include "hermes/qmi_constants.h"

namespace {
// This allows testing of EsimQmiImpl without actually needing to open a real
// QRTR socket to a QRTR modem.
bool CreateSocketPair(base::ScopedFD* one, base::ScopedFD* two) {
  int raw_socks[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, raw_socks) != 0) {
    PLOG(ERROR) << "Failed to create socket pair";
    return false;
  }
  one->reset(raw_socks[0]);
  two->reset(raw_socks[1]);
  return true;
}
}  // namespace

namespace hermes {

EsimQmiImpl::EsimQmiImpl(uint8_t slot, base::ScopedFD fd)
    : watcher_(FROM_HERE),
      slot_(slot),
      buffer_(4096),
      node_(-1),
      port_(-1),
      channel_(-1),
      qrtr_socket_fd_(std::move(fd)),
      weak_factory_(this) {}

void EsimQmiImpl::Initialize(const base::Closure& success_callback,
                             const ErrorCallback& error_callback) {
  bool ret = base::MessageLoopForIO::current()->WatchFileDescriptor(
      qrtr_socket_fd_.get(), true /* persistant */,
      base::MessageLoopForIO::WATCH_READ, &watcher_, this);
  CHECK(ret);
  qrtr_new_lookup(qrtr_socket_fd_.get(), kQrtrUimService, 1 /* version */,
                  0 /* instance */);
  initialize_callback_ = success_callback;
}

// static
std::unique_ptr<EsimQmiImpl> EsimQmiImpl::Create() {
  base::ScopedFD fd(qrtr_open(kQrtrPort));
  if (!fd.is_valid()) {
    return nullptr;
  }
  return base::WrapUnique(new EsimQmiImpl(kEsimSlot, std::move(fd)));
}

// static
std::unique_ptr<EsimQmiImpl> EsimQmiImpl::CreateForTest(base::ScopedFD* sock) {
  base::ScopedFD fd;
  if (!CreateSocketPair(&fd, sock)) {
    return nullptr;
  }

  return base::WrapUnique(new EsimQmiImpl(kEsimSlot, std::move(fd)));
}

void EsimQmiImpl::OpenLogicalChannel(const DataCallback& data_callback,
                                     const ErrorCallback& error_callback) {
  SendEsimMessage(QmiCommand::kOpenLogicalChannel, data_callback,
                  error_callback);
}

// TODO(jruthe): pass |which| to EsimQmiImpl::SendEsimMessage to make the
// correct libqrtr call to the eSIM chip.
void EsimQmiImpl::GetInfo(int which,
                          const DataCallback& data_callback,
                          const ErrorCallback& error_callback) {
  if (!qrtr_socket_fd_.is_valid()) {
    error_callback.Run(EsimError::kEsimNotConnected);
    return;
  }

  if (which != kEsimInfo1) {
    error_callback.Run(EsimError::kEsimError);
    return;
  }

  SendEsimMessage(QmiCommand::kSendApdu, data_callback, error_callback);
}

void EsimQmiImpl::GetChallenge(const DataCallback& data_callback,
                               const ErrorCallback& error_callback) {
  if (!qrtr_socket_fd_.is_valid()) {
    error_callback.Run(EsimError::kEsimNotConnected);
    return;
  }

  SendEsimMessage(QmiCommand::kSendApdu, data_callback, error_callback);
}

// TODO(jruthe): pass |server_data| to EsimQmiImpl::SendEsimMessage to make
// correct libqrtr call to the eSIM chip.
void EsimQmiImpl::AuthenticateServer(const DataBlob& server_data,
                                     const DataCallback& data_callback,
                                     const ErrorCallback& error_callback) {
  if (!qrtr_socket_fd_.is_valid()) {
    error_callback.Run(EsimError::kEsimNotConnected);
    return;
  }

  SendEsimMessage(QmiCommand::kSendApdu, data_callback, error_callback);
}

void EsimQmiImpl::SendEsimMessage(const QmiCommand command,
                                  const DataBlob& data,
                                  const DataCallback& data_callback,
                                  const ErrorCallback& error_callback) const {
  DataBlob result_code_tlv;
  switch (command) {
    case QmiCommand::kOpenLogicalChannel:
      // std::vector<uint8_t> slot_tlv = {0x01, 0x01, 0x00, 0x01};
      // TODO(jruthe): insert actual PostTask for QMI call here to open logical
      // channel and populate result_code_tlv with return data from
      // SEND_APDU_IND QMI callback
      //
      result_code_tlv = {0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
      data_callback.Run(result_code_tlv);
      break;
    case QmiCommand::kLogicalChannel:
      // TODO(jruthe) insert PostTask for closing logical channel
      result_code_tlv = {0x00};
      data_callback.Run(result_code_tlv);
      break;
    case QmiCommand::kSendApdu:
      // TODO(jruthe): implement some logic to construct different APDUs.

      // TODO(jruthe): insert actual PostTask for SEND_APDU QMI call
      result_code_tlv = {0x00};
      data_callback.Run(result_code_tlv);
      break;
  }
}

void EsimQmiImpl::SendEsimMessage(const QmiCommand command,
                                  const DataCallback& data_callback,
                                  const ErrorCallback& error_callback) const {
  SendEsimMessage(command, DataBlob(), data_callback, error_callback);
}

void EsimQmiImpl::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(qrtr_socket_fd_.get(), fd);

  uint32_t node, port;

  int bytes_received =
      qrtr_recvfrom(qrtr_socket_fd_.get(), buffer_.data(), buffer_.size(),
                    &node, &port);
  if (bytes_received < 0) {
    LOG(ERROR) << "qrtr_recvfrom failed";
    return;
  }

  sockaddr_qrtr qrtr_sock;
  qrtr_sock.sq_family = AF_QIPCRTR;
  qrtr_sock.sq_node = node;
  qrtr_sock.sq_port = port;

  qrtr_packet pkt;
  int ret = qrtr_decode(&pkt, buffer_.data(), bytes_received, &qrtr_sock);
  if (ret < 0) {
    LOG(ERROR) << "qrtr_decode failed";
    return;
  }

  if (pkt.data_len == 0) {
    return;
  }

  // TODO(jruthe): parse qrtr packet into different responses
  switch (pkt.type) {
    case QRTR_TYPE_NEW_SERVER:
      if (pkt.service == kQrtrUimService && channel_ == kInvalidChannel) {
        port_ = pkt.port;
        node_ = pkt.node;
        initialize_callback_.Run();
      }
      break;
    default:
      DLOG(INFO) << "unkown QRTR packet type";
      break;
  }
}

void EsimQmiImpl::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

}  // namespace hermes
