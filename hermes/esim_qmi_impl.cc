// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/esim_qmi_impl.h"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/strings/string_number_conversions.h>

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

// Convert ASCII-encoded hex character to its corresponding digit
bool HexToDigit(uint8_t hex_char, uint8_t* digit) {
  DLOG_ASSERT(digit);
  if ('0' <= hex_char && hex_char <= '9') {
    *digit = hex_char - '0';
  } else if ('A' <= hex_char && hex_char <= 'F') {
    *digit = hex_char - 'A' + 10;
  } else if ('a' <= hex_char && hex_char <= 'f') {
    *digit = hex_char - 'a' + 10;
  } else {
    return false;
  }
  return true;
}
}  // namespace

namespace hermes {

// Maximum size of an APDU data packet.
constexpr size_t kMaxApduDataSize = 255;

// P1 byte based on the length of the data field P3 in a transport command.
constexpr uint8_t kP1MoreBlocks = 0x11;
constexpr uint8_t kP1LastBlock = 0x91;

// Status byte for response okay as defined in ISO 7816.
constexpr uint8_t kApduStatusOkay = 0x90;

// Status byte for more response as defined in ISO 7816.
constexpr uint8_t kApduStatusMoreResponse = 0x61;

// Command code to request more bytes from the chip.
constexpr uint8_t kGetMoreResponseCommand = 0xC0;

// Store data class as defined in ISO 7816.
constexpr uint8_t kClaStoreData = 0x80;

// Store data instruction as defined in ISO 7816.
constexpr uint8_t kInsStoreData = 0xE2;

// Indicator that the length field will be two bytes, instead of one as defined
// in ISO 7816.
constexpr uint8_t kTwoByteLengthIndicator = 0x82;

// Le byte as defined in SGP.22
constexpr uint8_t kLeByte = 0x00;

// ASN.1 Tags for primitive types
constexpr uint8_t kAsn1TagCtx0 = 0x80;
constexpr uint8_t kAsn1TagCtx2 = 0x82;

// ASN.1 Tags for constructed types
constexpr uint8_t kAsn1TagCtxCmp0 = 0xA0;
constexpr uint8_t kAsn1TagCtxCmp1 = 0xA1;

// Application identifier for opening a logical channel to the eSIM slot
constexpr std::array<uint8_t, 16> kIsdrAid = {
    0xA0, 0x00, 0x00, 0x05, 0x59, 0x10, 0x10, 0xFF,
    0xFF, 0xFF, 0xFF, 0x89, 0x00, 0x00, 0x01, 0x00,
};

EsimQmiImpl::EsimQmiImpl(uint8_t slot,
                         const std::string& imei,
                         const std::string& matching_id,
                         base::ScopedFD fd)
    : watcher_(FROM_HERE),
      current_transaction_(1),
      imei_(imei),
      matching_id_(matching_id),
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
  int success = qrtr_new_lookup(qrtr_socket_fd_.get(), kQrtrUimService,
                                1 /* version */, 0 /* instance */);
  if (success < 0) {
    LOG(ERROR) << __func__ << ": qrtr_new_lookup failed";
    error_callback.Run(EsimError::kEsimError);
    return;
  }
  initialize_callback_ = success_callback;
}

// static
std::unique_ptr<EsimQmiImpl> EsimQmiImpl::Create(std::string imei,
                                                 std::string matching_id) {
  base::ScopedFD fd(qrtr_open(kQrtrPort));
  if (!fd.is_valid()) {
    LOG(ERROR) << __func__ << ": Could not open socket";
    return nullptr;
  }

  VLOG(1) << "Constructing Esim object with slot : "
          << static_cast<int>(kEsimSlot) << " and imei " << imei;

  return base::WrapUnique(
      new EsimQmiImpl(kEsimSlot, imei, matching_id, std::move(fd)));
}

// static
std::unique_ptr<EsimQmiImpl> EsimQmiImpl::CreateForTest(
    base::ScopedFD* sock, std::string imei, std::string matching_id) {
  base::ScopedFD fd;
  if (!CreateSocketPair(&fd, sock)) {
    LOG(ERROR) << __func__ << ": Could not open socket";
    return nullptr;
  }

  VLOG(1) << __func__ << ": Constructing test Esim object with slot : "
          << static_cast<int>(kEsimSlot) << " and imei " << imei;

  return base::WrapUnique(
      new EsimQmiImpl(kEsimSlot, imei, matching_id, std::move(fd)));
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
    LOG(ERROR) << __func__ << ": qmi_encode_message failed";
    error_callback.Run(EsimError::kEsimError);
    return;
  }

  VLOG(1) << __func__
          << ": Initiating OpenLogicalChannel Transaction with request (size : "
          << buffer.data_len
          << ") : " << base::HexEncode(buffer.data, buffer.data_len);
  InitiateTransaction(buffer, data_callback, error_callback);
}

void EsimQmiImpl::GetInfo(int which,
                          const DataCallback& data_callback,
                          const ErrorCallback& error_callback) {
  if (!qrtr_socket_fd_.is_valid()) {
    LOG(ERROR) << __func__ << ": File Descriptor to QRTR invalid";
    error_callback.Run(EsimError::kEsimNotConnected);
    return;
  }

  const std::vector<uint8_t> get_info_request = {
      static_cast<uint8_t>((which >> 8) & 0xFF),
      static_cast<uint8_t>(which & 0xFF),
      0x00,  // APDU length
  };

  payload_.clear();

  VLOG(1) << __func__
          << ": Added GetInfo APDU (size : " << get_info_request.size()
          << ") to queue : "
          << base::HexEncode(get_info_request.data(), get_info_request.size());
  QueueStoreData(get_info_request);

  SendApdu(data_callback, error_callback);
}

void EsimQmiImpl::GetChallenge(const DataCallback& data_callback,
                               const ErrorCallback& error_callback) {
  if (!qrtr_socket_fd_.is_valid()) {
    LOG(ERROR) << __func__ << ": File Descriptor to QRTR invalid";
    error_callback.Run(EsimError::kEsimNotConnected);
    return;
  }

  const std::vector<uint8_t> get_challenge_request = {
      static_cast<uint8_t>((kEsimChallengeTag >> 8) & 0xFF),
      static_cast<uint8_t>(kEsimChallengeTag & 0xFF),
      0x00,  // APDU Length
  };
  payload_.clear();

  VLOG(1) << __func__ << ": Added GetChallenge APDU (size : "
          << get_challenge_request.size() << ") to queue : "
          << base::HexEncode(get_challenge_request.data(),
                             get_challenge_request.size());
  QueueStoreData(get_challenge_request);

  SendApdu(data_callback, error_callback);
}

// TODO(jruthe): pass |server_data| to EsimQmiImpl::SendEsimMessage to make
// correct libqrtr call to the eSIM chip.
void EsimQmiImpl::AuthenticateServer(
    const std::vector<uint8_t>& server_signed1,
    const std::vector<uint8_t>& server_signature,
    const std::vector<uint8_t>& public_key,
    const std::vector<uint8_t>& server_certificate,
    const DataCallback& data_callback,
    const ErrorCallback& error_callback) {
  if (!qrtr_socket_fd_.is_valid()) {
    LOG(ERROR) << __func__ << ": File Descriptor to QRTR invalid";
    error_callback.Run(EsimError::kEsimNotConnected);
    return;
  }

  std::vector<uint8_t> ctx_params;
  if (!ConstructCtxParams(&ctx_params)) {
    LOG(ERROR) << __func__ << ": failed to construct ctx_params";
    error_callback.Run(EsimError::kEsimError);
    return;
  }

  size_t payload_size = server_signed1.size() + server_signature.size() +
                        public_key.size() + server_certificate.size() +
                        ctx_params.size();

  std::vector<uint8_t> raw_data_buffer;
  raw_data_buffer.push_back(
      static_cast<uint8_t>((kAuthenticateServerTag >> 8) & 0xFF));
  raw_data_buffer.push_back(
      static_cast<uint8_t>(kAuthenticateServerTag & 0xFF));
  raw_data_buffer.push_back(kTwoByteLengthIndicator);
  raw_data_buffer.push_back(static_cast<uint8_t>((payload_size >> 8) & 0xFF));
  raw_data_buffer.push_back(static_cast<uint8_t>(payload_size & 0xFF));
  raw_data_buffer.insert(raw_data_buffer.end(), server_signed1.begin(),
                         server_signed1.end());
  raw_data_buffer.insert(raw_data_buffer.end(), server_signature.begin(),
                         server_signature.end());
  raw_data_buffer.insert(raw_data_buffer.end(), public_key.begin(),
                         public_key.end());
  raw_data_buffer.insert(raw_data_buffer.end(), server_certificate.begin(),
                         server_certificate.end());
  raw_data_buffer.insert(raw_data_buffer.end(), ctx_params.begin(),
                         ctx_params.end());

  payload_.clear();
  QueueStoreData(raw_data_buffer);

  SendApdu(data_callback, error_callback);
}

void EsimQmiImpl::QueueStoreData(const std::vector<uint8_t>& payload) {
  FragmentAndQueueApdu(kClaStoreData, kInsStoreData, payload);
}

void EsimQmiImpl::PrepareDownloadRequest(
    const std::vector<uint8_t>& smdp_signed2,
    const std::vector<uint8_t>& smdp_signature2,
    const std::vector<uint8_t>& smdp_certificate,
    const DataCallback& data_callback,
    const ErrorCallback& error_callback) {
  if (!qrtr_socket_fd_.is_valid()) {
    LOG(ERROR) << __func__ << ": File Descriptor to QRTR invalid";
    error_callback.Run(EsimError::kEsimNotConnected);
    return;
  }

  size_t payload_size =
      smdp_signed2.size() + smdp_signature2.size() + smdp_certificate.size();

  std::vector<uint8_t> raw_data_buffer;
  raw_data_buffer.push_back(
      static_cast<uint8_t>((kPrepareDownloadRequestTag >> 8) & 0xFF));
  raw_data_buffer.push_back(
      static_cast<uint8_t>(kPrepareDownloadRequestTag & 0xFF));

  // Magic number
  raw_data_buffer.push_back(0x82);
  raw_data_buffer.push_back(static_cast<uint8_t>((payload_size >> 8) & 0xFF));
  raw_data_buffer.push_back(static_cast<uint8_t>(payload_size & 0xFF));
  raw_data_buffer.insert(raw_data_buffer.end(), smdp_signed2.begin(),
                         smdp_signed2.end());
  raw_data_buffer.insert(raw_data_buffer.end(), smdp_signature2.begin(),
                         smdp_signature2.end());
  raw_data_buffer.insert(raw_data_buffer.end(), smdp_certificate.begin(),
                         smdp_certificate.end());

  payload_.clear();
  QueueStoreData(raw_data_buffer);

  SendApdu(data_callback, error_callback);
}

void EsimQmiImpl::FragmentAndQueueApdu(
    uint8_t cla, uint8_t ins, const std::vector<uint8_t>& apdu_payload) {
  size_t total_packets = std::max(1u,
      (apdu_payload.size() + kMaxApduDataSize - 1) / kMaxApduDataSize);

  VLOG(1) << __func__ << ": constructing " << total_packets << " packets";

  auto front_iterator = apdu_payload.begin();
  size_t i;
  for (i = 0; i < total_packets - 1; ++i) {
    std::vector<uint8_t> buf{cla, ins, kP1MoreBlocks, static_cast<uint8_t>(i),
                             kMaxApduDataSize};
    buf.reserve(kMaxApduDataSize + 5);
    buf.insert(buf.end(), front_iterator, front_iterator + kMaxApduDataSize);
    VLOG(1) << __func__ << ": Queuing APDU fragment (size : " << buf.size()
            << ") : " << base::HexEncode(buf.data(), buf.size());
    apdu_queue_.push_back(std::move(buf));
    std::advance(front_iterator, kMaxApduDataSize);
  }
  uint8_t final_packet_size = apdu_payload.end() - front_iterator;
  std::vector<uint8_t> buf{cla, ins, kP1LastBlock, static_cast<uint8_t>(i),
                           final_packet_size};
  buf.reserve(final_packet_size + 6);
  buf.insert(buf.end(), front_iterator, front_iterator + final_packet_size);
  buf.push_back(kLeByte);
  VLOG(1) << __func__ << ": Queuing final APDU fragment (size : " << buf.size()
          << ") : " << base::HexEncode(buf.data(), buf.size());
  apdu_queue_.push_back(std::move(buf));
}

void EsimQmiImpl::SendApdu(const DataCallback& data_callback,
                           const ErrorCallback& error_callback) {
  if (apdu_queue_.empty()) {
    LOG(ERROR) << __func__ << ": called with empty queue";
    error_callback.Run(EsimError::kEsimError);
    return;
  }

  std::vector<uint8_t> raw_buffer(kMaxApduDataSize * 2);
  std::vector<uint8_t> data_buffer = apdu_queue_.front();
  apdu_queue_.pop_front();

  qrtr_packet buffer;
  buffer.data = raw_buffer.data();
  buffer.data_len = raw_buffer.size();

  uim_send_apdu_req request;
  request.slot = slot_;
  request.channel_id_valid = true;
  request.channel_id = channel_;
  request.apdu_len = data_buffer.size();

  std::copy(data_buffer.begin(), data_buffer.end(), request.apdu);

  size_t len = qmi_encode_message(
      &buffer, QMI_REQUEST, static_cast<uint32_t>(QmiUimCommand::kSendApdu),
      current_transaction_, &request, uim_send_apdu_req_ei);

  if (len < 0) {
    LOG(ERROR) << __func__ << ": qmi_encode_message failed";
    error_callback.Run(EsimError::kEsimError);
    return;
  }

  VLOG(1) << __func__ << ": Initiating APDU Transaction with buffer (size : "
          << buffer.data_len
          << ") : " << base::HexEncode(buffer.data, buffer.data_len);

  InitiateTransaction(buffer, data_callback, error_callback);
}

void EsimQmiImpl::InitiateTransaction(const qrtr_packet& packet,
                                      const DataCallback& data_callback,
                                      const ErrorCallback& error_callback) {
  int bytes_sent = qrtr_sendto(qrtr_socket_fd_.get(), node_, port_, packet.data,
                               packet.data_len);

  if (bytes_sent < 0) {
    LOG(ERROR) << __func__ << ": qrtr_sendto failed";
    error_callback.Run(EsimError::kEsimError);
    return;
  }

  VLOG(1) << __func__ << ": Packet sent to eSIM, saving callbacks";

  response_callbacks_[current_transaction_++] =
      TransactionCallback(data_callback, error_callback);
}

void EsimQmiImpl::FinalizeTransaction(const qrtr_packet& packet) {
  uint32_t qmi_type;
  int ret = qmi_decode_header(&packet, &qmi_type);
  if (ret < 0) {
    LOG(ERROR) << __func__ << ": Got invalid data packet.";
    return;
  }
  uint32_t transaction_number = GetTransactionNumber(packet);

  if (transaction_number > current_transaction_ || transaction_number == 0) {
    LOG(ERROR) << __func__
               << ": Got invalid transaction number : " << transaction_number;
    return;
  }

  const auto& callbacks = response_callbacks_.find(transaction_number);
  if (callbacks == response_callbacks_.end()) {
    LOG(ERROR) << __func__ << ": Couldn't find transaction "
               << transaction_number;
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
        LOG(ERROR) << __func__ << ": APDU response invalid.";

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
            static_cast<uint8_t>(kClaStoreData | channel_),
            kGetMoreResponseCommand, 0x00, 0x00, sw2_};

        apdu_queue_.push_back(get_more_request);
        SendApdu(transaction_callbacks.data_callback,
                 transaction_callbacks.error_callback);
        return;
      } else if (payload_.empty() && sw1_ == kApduStatusOkay) {
        SendApdu(transaction_callbacks.data_callback,
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
      LOG(WARNING) << __func__ << ": Unknown QMI data type: " << qmi_type;
      break;
  }
}

bool EsimQmiImpl::StringToBcdBytes(const std::string& source,
                                   std::vector<uint8_t>* dest) {
  DLOG_ASSERT(dest);
  dest->reserve((source.size() + 1) / 2);
  size_t i = 0;
  uint8_t msb, lsb;
  for (; i < source.size() / 2; ++i) {
    if (!HexToDigit(source[2*i], &lsb) ||
        !HexToDigit(source[2*i + 1], &msb)) {
      return false;
    }
    dest->push_back((msb << 4) | lsb);
  }
  if (source.size() % 2) {
    if (!HexToDigit(source[2*i - 1], &lsb)) {
      return false;
    }
    dest->push_back(lsb);
  }
  return true;
}

bool EsimQmiImpl::ConstructCtxParams(std::vector<uint8_t>* ctx_params) {
  const std::vector<uint8_t> device_caps = {
      0x80, 0x03, 0x0D, 0x00, 0x00, 0x81, 0x03, 0x0D, 0x00, 0x00,
      0x85, 0x03, 0x0D, 0x00, 0x00, 0x87, 0x03, 0x02, 0x02, 0x00,
  };

  std::vector<uint8_t> imei_bytes;
  if (!StringToBcdBytes(imei_, &imei_bytes)) {
    LOG(ERROR) << __func__ << ": failed to convert imei (" << imei_
               << ") to BCD format";
    return false;
  }

  std::vector<uint8_t> tac(imei_bytes.begin(), imei_bytes.begin() + 4);

  VLOG(1) << "converted imei : " << base::HexEncode(imei_.data(), imei_.size());

  std::vector<uint8_t> matching_id_bytes;
  if (!StringToBcdBytes(matching_id_, &matching_id_bytes)) {
    LOG(ERROR) << __func__ << ": failed to convert matching_id ("
               << matching_id_ << ") to BCD format";
    return false;
  }

  uint8_t total_len = matching_id_bytes.size() + device_caps.size() +
                      imei_bytes.size() + tac.size() + 10;

  ctx_params->push_back(kAsn1TagCtxCmp0);
  ctx_params->push_back(total_len);

  ctx_params->push_back(kAsn1TagCtx0);
  ctx_params->push_back(static_cast<uint8_t>(matching_id_bytes.size()));
  ctx_params->insert(ctx_params->end(), matching_id_bytes.begin(),
                     matching_id_bytes.end());
  ctx_params->push_back(kAsn1TagCtxCmp1);
  ctx_params->push_back(
      static_cast<uint8_t>(tac.size() + device_caps.size() + imei_.size()) + 6);
  ctx_params->push_back(kAsn1TagCtx0);
  ctx_params->push_back(static_cast<uint8_t>(tac.size()));
  ctx_params->insert(ctx_params->end(), tac.begin(), tac.end());
  ctx_params->push_back(kAsn1TagCtxCmp0);
  ctx_params->push_back(static_cast<uint8_t>(device_caps.size()));
  ctx_params->insert(ctx_params->end(), device_caps.begin(), device_caps.end());
  ctx_params->push_back(kAsn1TagCtx2);
  ctx_params->push_back(static_cast<uint8_t>(imei_.size()));
  ctx_params->insert(ctx_params->end(), imei_.begin(), imei_.end());

  VLOG(1) << "ctxParams : "
          << base::HexEncode(ctx_params->data(), ctx_params->size());

  return true;
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
  return (sw1_ == kApduStatusMoreResponse);
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
    LOG(ERROR) << __func__ << ": Recvfrom failed.";
    return;
  }

  sockaddr_qrtr qrtr_sock;
  qrtr_sock.sq_family = AF_QIPCRTR;
  qrtr_sock.sq_node = node;
  qrtr_sock.sq_port = port;

  qrtr_packet pkt;
  int ret = qrtr_decode(&pkt, buffer_.data(), bytes_received, &qrtr_sock);
  if (ret < 0) {
    LOG(ERROR) << __func__ << ": Qrtr_decode failed.";
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
      VLOG(1)
          << __func__
          << ": calling EsimQmiImpl::FinalizeTransaction with packet (size : "
          << pkt.data_len << ") : " << base::HexEncode(pkt.data, pkt.data_len);
      FinalizeTransaction(pkt);
      break;
    default:
      LOG(WARNING) << __func__ << ": Unkown QRTR packet type: " << pkt.type;
      break;
  }
}

void EsimQmiImpl::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

}  // namespace hermes
