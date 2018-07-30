// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/esim_qmi_impl.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/memory/ptr_util.h>

namespace {
// This allows testing of EsimQmiImpl without actually needing to open a real
// QRTR socket to a QRTR modem.
bool CreateSocketPair(base::ScopedFD* one, base::ScopedFD* two) {
  int raw_socks[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, raw_socks) != 0) {
    PLOG(ERROR) << "Failed to create socket pair.";
    return false;
  }
  one->reset(raw_socks[0]);
  two->reset(raw_socks[1]);
  return true;
}
}  // namespace

namespace hermes {

// Status byte for more response as defined in ISO 7816.
constexpr uint8_t kSw1MoreResponse = 0x61;

// Command code to request more bytes from the chip.
constexpr uint8_t kGetMoreResponseCommand = 0xC0;

// Store data class
constexpr uint8_t kStoreDataCla = 0x80;

// Application identifier for opening a logical channel to the eSIM slot
constexpr std::array<uint8_t, 16> kIsdrAid = {
    0xA0, 0x00, 0x00, 0x05, 0x59, 0x10, 0x10, 0xFF,
    0xFF, 0xFF, 0xFF, 0x89, 0x00, 0x00, 0x01, 0x00,
};

EsimQmiImpl::EsimQmiImpl(uint8_t slot, base::ScopedFD fd)
    : watcher_(FROM_HERE),
      current_transaction_(1),
      slot_(slot),
      buffer_(4096),
      node_(-1),
      port_(-1),
      channel_(-1),
      qrtr_socket_fd_(std::move(fd)),
      weak_factory_(this) {}

EsimQmiImpl::TransactionCallback::TransactionCallback(
    const DataCallback& data_callback, const ErrorCallback& error_callback)
    : data_callback(data_callback), error_callback(error_callback) {}

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
  std::vector<uint8_t> raw_buffer(kBufferDataSize, 0);

  qrtr_packet buffer;
  buffer.data_len = raw_buffer.size();
  buffer.data = raw_buffer.data();

  uim_open_logical_channel_req request;
  request.slot = slot_;
  request.aid_valid = true;
  request.aid_len = kIsdrAid.size();
  std::copy(kIsdrAid.begin(), kIsdrAid.end(), request.aid);

  size_t len = qmi_encode_message(
      &buffer, QMI_REQUEST,
      static_cast<uint32_t>(QmiUimCommand::kOpenLogicalChannel),
      current_transaction_, &request, uim_open_logical_channel_req_ei);

  if (len < 0) {
    error_callback.Run(EsimError::kEsimError);
    return;
  }

  InitiateTransaction(buffer, data_callback, error_callback);
}

void EsimQmiImpl::GetInfo(int which,
                          const DataCallback& data_callback,
                          const ErrorCallback& error_callback) {
  if (!qrtr_socket_fd_.is_valid()) {
    error_callback.Run(EsimError::kEsimNotConnected);
    return;
  }

  const std::vector<uint8_t> get_info_request = {
      static_cast<uint8_t>(kStoreDataCla | channel_),
      0xE2,
      0x91,
      0x00,
      0x03,
      static_cast<uint8_t>((which >> 8) & 0xFF),
      static_cast<uint8_t>(which & 0xFF),
      0x00,
      0x00,
  };
  payload_.clear();
  SendApdu(get_info_request, data_callback, error_callback);
}

void EsimQmiImpl::GetChallenge(const DataCallback& data_callback,
                               const ErrorCallback& error_callback) {
  if (!qrtr_socket_fd_.is_valid()) {
    error_callback.Run(EsimError::kEsimNotConnected);
    return;
  }

  const std::vector<uint8_t> get_challenge_request = {
      static_cast<uint8_t>(kStoreDataCla | channel_),
      0xE2,
      0x91,
      0x00,
      0x03,
      0xBF,
      0x2E,
      0x00,
      0x00,
  };
  payload_.clear();
  SendApdu(get_challenge_request, data_callback, error_callback);
}

// TODO(jruthe): pass |server_data| to EsimQmiImpl::SendEsimMessage to make
// correct libqrtr call to the eSIM chip.
void EsimQmiImpl::AuthenticateServer(const std::vector<uint8_t>& server_data,
                                     const DataCallback& data_callback,
                                     const ErrorCallback& error_callback) {
  if (!qrtr_socket_fd_.is_valid()) {
    error_callback.Run(EsimError::kEsimNotConnected);
    return;
  }

  payload_.clear();
  // TODO(jruthe): Add AuthenticateServer Tag to qmi_constants.
  // SendApdu(QmiUimCommand::kSendApdu, data_callback, error_callback);
}

void EsimQmiImpl::SendApdu(const std::vector<uint8_t>& data_buffer,
                           const DataCallback& data_callback,
                           const ErrorCallback& error_callback) {
  size_t len;
  std::vector<uint8_t> raw_buffer(kBufferDataSize);

  qrtr_packet buffer;
  buffer.data = raw_buffer.data();
  buffer.data_len = raw_buffer.size();

  uim_send_apdu_req request;
  request.slot = slot_;
  request.channel_id_valid = true;
  request.channel_id = channel_;
  request.apdu_len = data_buffer.size();
  std::copy(data_buffer.begin(), data_buffer.end(), request.apdu);

  len = qmi_encode_message(
      &buffer, QMI_REQUEST, static_cast<uint32_t>(QmiUimCommand::kSendApdu),
      current_transaction_, &request, uim_send_apdu_req_ei);

  if (len < 0) {
    error_callback.Run(EsimError::kEsimError);
    return;
  }

  InitiateTransaction(buffer, data_callback, error_callback);
}

void EsimQmiImpl::InitiateTransaction(const qrtr_packet& packet,
                                      const DataCallback& data_callback,
                                      const ErrorCallback& error_callback) {
  int bytes_sent = qrtr_sendto(qrtr_socket_fd_.get(), node_, port_, packet.data,
                               packet.data_len);

  if (bytes_sent < 0) {
    error_callback.Run(EsimError::kEsimError);
    return;
  }

  response_callbacks_[current_transaction_++] =
      TransactionCallback(data_callback, error_callback);
}

void EsimQmiImpl::FinalizeTransaction(const qrtr_packet& packet) {
  uint32_t qmi_type;
  int ret = qmi_decode_header(&packet, &qmi_type);
  if (ret < 0) {
    LOG(ERROR) << "Got invalid data packet.";
    return;
  }
  uint32_t transaction_number = GetTransactionNumber(packet);

  if (transaction_number > current_transaction_ || transaction_number == 0) {
    LOG(ERROR) << "Got invalid transaction number.";
    return;
  }

  const auto& callbacks = response_callbacks_.find(transaction_number);
  if (callbacks == response_callbacks_.end()) {
    LOG(ERROR) << "Couldn't find transaction " << transaction_number;
    return;
  }

  const TransactionCallback& transaction_callbacks = callbacks->second;

  switch (qmi_type) {
    case static_cast<uint32_t>(QmiUimCommand::kOpenLogicalChannel): {
      uim_open_logical_channel_resp resp;
      qmi_decode_message(
          &resp, &transaction_number, &packet, QMI_RESPONSE,
          static_cast<uint32_t>(QmiUimCommand::kOpenLogicalChannel),
          uim_open_logical_channel_resp_ei);
      if (!ResponseSuccess(resp.result.result)) {
        LOG(ERROR) << "QmiUimCommand::kOpenLogicalChannel failed.";

        // TODO(jruthe): maybe change this to clear instead?
        response_callbacks_.erase(callbacks);
        transaction_callbacks.error_callback.Run(EsimError::kEsimError);
        return;
      }

      if (resp.channel_id_valid) {
        channel_ = resp.channel_id;
      } else {
        LOG(ERROR) << "Invalid channel_id.";

        // TODO(jruthe): ditto to above?
        response_callbacks_.erase(callbacks);
        transaction_callbacks.error_callback.Run(EsimError::kEsimError);
        return;
      }

      transaction_callbacks.data_callback.Run(std::vector<uint8_t>());
      break;
    }
    case static_cast<uint32_t>(QmiUimCommand::kSendApdu): {
      uim_send_apdu_resp resp;
      qmi_decode_message(&resp, &transaction_number, &packet, QMI_RESPONSE,
                         static_cast<uint32_t>(QmiUimCommand::kSendApdu),
                         uim_send_apdu_resp_ei);
      if (!ResponseSuccess(resp.result.result)) {
        LOG(ERROR) << "Response invalid.";

        // TODO(jruthe): ditto
        response_callbacks_.erase(callbacks);
        transaction_callbacks.error_callback.Run(EsimError::kEsimError);
        return;
      }
      payload_.insert(payload_.end(), resp.apdu_response,
                      resp.apdu_response + resp.apdu_response_len);
      sw1_ = payload_[payload_.size() - 2];
      sw2_ = payload_[payload_.size() - 1];
      payload_.pop_back();
      payload_.pop_back();

      if (MorePayloadIncoming()) {
        const std::vector<uint8_t> get_more_request = {
            static_cast<uint8_t>(kStoreDataCla | channel_),
            kGetMoreResponseCommand, 0x00, 0x00, sw2_};

        SendApdu(get_more_request, transaction_callbacks.data_callback,
                 transaction_callbacks.error_callback);
        return;
      }
      transaction_callbacks.data_callback.Run(payload_);
      response_callbacks_.erase(callbacks);
      break;
    }
    case static_cast<uint32_t>(QmiUimCommand::kReset):
      break;
    default:
      DLOG(INFO) << "Unknown QMI data type: " << qmi_type;
      break;
  }
}

uint16_t EsimQmiImpl::GetTransactionNumber(const qrtr_packet& packet) const {
  if (packet.data_len < 3)
    return 0;
  const uint8_t* data = static_cast<uint8_t*>(packet.data);
  return ((data[2] << 8) | (data[1]));
}

bool EsimQmiImpl::ResponseSuccess(uint16_t response) const {
  return (response == 0);
}

bool EsimQmiImpl::MorePayloadIncoming() const {
  return (sw1_ == kSw1MoreResponse);
}

uint8_t EsimQmiImpl::GetNextPayloadSize() const {
  return sw2_;
}

void EsimQmiImpl::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(qrtr_socket_fd_.get(), fd);

  uint32_t node, port;

  int bytes_received = qrtr_recvfrom(qrtr_socket_fd_.get(), buffer_.data(),
                                     buffer_.size(), &node, &port);
  if (bytes_received < 0) {
    LOG(ERROR) << "Recvfrom failed.";
    return;
  }

  sockaddr_qrtr qrtr_sock;
  qrtr_sock.sq_family = AF_QIPCRTR;
  qrtr_sock.sq_node = node;
  qrtr_sock.sq_port = port;

  qrtr_packet pkt;
  int ret = qrtr_decode(&pkt, buffer_.data(), bytes_received, &qrtr_sock);
  if (ret < 0) {
    LOG(ERROR) << "Qrtr_decode failed.";
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
    case QRTR_TYPE_DATA:
      FinalizeTransaction(pkt);
      break;
    default:
      DLOG(INFO) << "Unkown QRTR packet type: " << pkt.type;
      break;
  }
}

void EsimQmiImpl::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

}  // namespace hermes
