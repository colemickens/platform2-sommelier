// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <utility>

#include <base/bind.h>
#include <base/callback.h>
#include <base/optional.h>
#include <base/strings/string_number_conversions.h>
#include <base/sys_byteorder.h>
#include <base/timer/timer.h>
#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <trunks/cr50_headers/u2f.h>

#include "u2fd/u2f_adpu.h"
#include "u2fd/u2fhid.h"
#include "u2fd/user_state.h"
#include "u2fd/util.h"

namespace {

// Size of the payload for an INIT U2F HID report.
constexpr size_t kInitReportPayloadSize = 57;
// Size of the payload for a Continuation U2F HID report.
constexpr size_t kContReportPayloadSize = 59;

constexpr uint8_t kInterfaceVersion = 2;

constexpr int kU2fHidTimeoutMs = 500;

// Maximum duration one can keep the channel lock as specified by the U2FHID
// specification
constexpr int kMaxLockDurationSeconds = 10;

// Response to the APDU requesting the U2F protocol version
constexpr char kSupportedU2fVersion[] = "U2F_V2";

// HID report descriptor for U2F interface.
constexpr uint8_t kU2fReportDesc[] = {
    0x06, 0xD0, 0xF1, /* Usage Page (FIDO Alliance), FIDO_USAGE_PAGE */
    0x09, 0x01,       /* Usage (U2F HID Auth. Device) FIDO_USAGE_U2FHID */
    0xA1, 0x01,       /* Collection (Application), HID_APPLICATION */
    0x09, 0x20,       /*  Usage (Input Report Data), FIDO_USAGE_DATA_IN */
    0x15, 0x00,       /*  Logical Minimum (0) */
    0x26, 0xFF, 0x00, /*  Logical Maximum (255) */
    0x75, 0x08,       /*  Report Size (8) */
    0x95, 0x40,       /*  Report Count (64), HID_INPUT_REPORT_BYTES */
    0x81, 0x02,       /*  Input (Data, Var, Abs), Usage */
    0x09, 0x21,       /*  Usage (Output Report Data), FIDO_USAGE_DATA_OUT */
    0x15, 0x00,       /*  Logical Minimum (0) */
    0x26, 0xFF, 0x00, /*  Logical Maximum (255) */
    0x75, 0x08,       /*  Report Size (8) */
    0x95, 0x40,       /*  Report Count (64), HID_OUTPUT_REPORT_BYTES */
    0x91, 0x02,       /*  Output (Data, Var, Abs), Usage */
    0xC0              /* End Collection */
};

// Vendor Command Return Codes.
constexpr uint32_t kVendorCmdRcSuccess = 0x000;
constexpr uint32_t kVendorCmdRcNotAllowed = 0x507;
constexpr uint32_t kVendorCmdRcPasswordRequired = 0x50a;

}  // namespace

namespace u2f {

class U2fHid::HidPacket {
 public:
  explicit HidPacket(const std::string& report);

  bool IsValidFrame() const { return valid_; }

  bool IsInitFrame() const { return (tcs_ & kFrameTypeMask) == kFrameTypeInit; }

  uint32_t ChannelId() const { return cid_; }

  U2fHid::U2fHidCommand Command() const {
    return static_cast<U2fHidCommand>(tcs_ & ~kFrameTypeMask);
  }

  uint8_t SeqNumber() const { return tcs_ & ~kFrameTypeMask; }

  int PayloadIndex() const { return IsInitFrame() ? 8 : 6; }

  size_t MessagePayloadSize() const { return bcnt_; }

 private:
  bool valid_;
  uint32_t cid_;   // Channel Identifier
  uint8_t tcs_;    // type and command or sequence number
  uint16_t bcnt_;  // payload length as defined by U2fHID specification
};

U2fHid::HidPacket::HidPacket(const std::string& report)
    : valid_(false), cid_(0), tcs_(0), bcnt_(0) {
  // the report is prefixed by the report ID (we skip it below).
  if (report.size() != kU2fReportSize + 1) /* Invalid U2FHID report */
    return;

  // U2FHID frame bytes parsing.
  // As defined in the "FIDO U2F HID Protocol Specification":
  // An initialization packet is defined as
  //
  // Offset Length  Mnemonic  Description
  // 0      4       CID       Channel identifier
  // 4      1       CMD       Command identifier (bit 7 always set)
  // 5      1       BCNTH     High part of payload length
  // 6      1       BCNTL     Low part of payload length
  // 7      (s - 7) DATA      Payload data (s is the fixed packet size)
  // The command byte has always the highest bit set to distinguish it
  // from a continuation packet, which is described below.
  //
  // A continuation packet is defined as
  //
  // Offset Length  Mnemonic  Description
  // 0      4       CID       Channel identifier
  // 4      1       SEQ       Packet sequence 0x00..0x7f (bit 7 always cleared)
  // 5      (s - 5) DATA      Payload data (s is the fixed packet size)
  // With this approach, a message with a payload less or equal to (s - 7)
  // may be sent as one packet. A larger message is then divided into one or
  // more continuation packets, starting with sequence number 0 which then
  // increments by one to a maximum of 127.

  // the CID word is not aligned
  memcpy(&cid_, &report[1], sizeof(cid_));
  tcs_ = report[5];

  uint16_t raw_count;
  memcpy(&raw_count, &report[6], sizeof(raw_count));
  bcnt_ = base::NetToHost16(raw_count);

  valid_ = true;
}

class U2fHid::HidMessage {
 public:
  HidMessage(U2fHidCommand cmd, uint32_t cid) : cid_(cid), cmd_(cmd) {}

  // Appends |bytes| to the message payload.
  void AddPayload(const std::string& bytes);

  // Appends the single |byte| to the message payload.
  void AddByte(uint8_t byte);

  // Fills an HID report with the part of the message starting at |offset|.
  // Returns the offset of the remaining unused content in the message.
  int BuildReport(int offset, std::string* report_out);

 private:
  uint32_t cid_;
  U2fHidCommand cmd_;
  std::string payload_;
};

void U2fHid::HidMessage::AddPayload(const std::string& bytes) {
  payload_ += bytes;
}

void U2fHid::HidMessage::AddByte(uint8_t byte) {
  payload_.push_back(byte);
}

int U2fHid::HidMessage::BuildReport(int offset, std::string* report_out) {
  size_t data_size;

  // Serialize one chunk of the message in a 64-byte HID report
  // (see the HID report structure in HidPacket constructor)
  report_out->assign(
      std::string(reinterpret_cast<char*>(&cid_), sizeof(uint32_t)));
  if (offset == 0) {  // INIT message
    uint16_t bcnt = payload_.size();
    report_out->push_back(static_cast<uint8_t>(cmd_) | kFrameTypeInit);
    report_out->push_back(bcnt >> 8);
    report_out->push_back(bcnt & 0xff);
    data_size = kInitReportPayloadSize;
  } else {  // CONT message
    // Insert sequence number.
    report_out->push_back((offset - kInitReportPayloadSize) /
                          kContReportPayloadSize);
    data_size = kContReportPayloadSize;
  }
  data_size = std::min(data_size, payload_.size() - offset);
  *report_out += payload_.substr(offset, data_size);
  // Ensure the report is 64-B long
  report_out->insert(report_out->end(), kU2fReportSize - report_out->size(), 0);
  offset += data_size;

  VLOG(2) << "TX RPT ["
          << base::HexEncode(report_out->data(), report_out->size()) << "]";

  return offset != payload_.size() ? offset : 0;
}

struct U2fHid::Transaction {
  uint32_t cid = 0;
  U2fHidCommand cmd = U2fHidCommand::kError;
  size_t total_size = 0;
  int seq = 0;
  std::string payload;
  base::OneShotTimer timeout;
};

U2fHid::U2fHid(std::unique_ptr<HidInterface> hid,
               const std::string& vendor_sysinfo,
               const bool g2f_mode,
               const bool user_secrets,
               const TpmAdpuCallback& adpu_fn,
               const TpmGenerateCallback& generate_fn,
               const TpmSignCallback& sign_fn,
               const TpmAttestCallback& attest_fn,
               const TpmG2fCertCallback& g2f_cert_fn,
               const IgnoreButtonCallback& ignore_fn,
               std::unique_ptr<UserState> user_state)
    : hid_(std::move(hid)),
      g2f_mode_(g2f_mode),
      user_secrets_(user_secrets),
      transmit_apdu_(adpu_fn),
      tpm_generate_(generate_fn),
      tpm_sign_(sign_fn),
      tpm_attest_(attest_fn),
      tpm_g2f_cert_(g2f_cert_fn),
      ignore_button_(ignore_fn),
      free_cid_(1),
      locked_cid_(0),
      user_state_(std::move(user_state)),
      vendor_sysinfo_(vendor_sysinfo) {
  transaction_ = std::make_unique<Transaction>();
  hid_->SetOutputReportHandler(
      base::Bind(&U2fHid::ProcessReport, base::Unretained(this)));
}

U2fHid::~U2fHid() = default;

bool U2fHid::Init() {
  return hid_->Init(kInterfaceVersion,
                    std::string(reinterpret_cast<const char*>(kU2fReportDesc),
                                sizeof(kU2fReportDesc)));
}

bool U2fHid::GetU2fVersion(std::string* version_out) {
  U2fCommandAdpu version_msg =
      U2fCommandAdpu::CreateForU2fIns(U2fIns::kU2fVersion);
  std::string version_resp_raw;

  int rc = transmit_apdu_.Run(version_msg.ToString(), &version_resp_raw);

  if (!rc) {
    // remove the 16-bit status code at the end
    *version_out = version_resp_raw.substr(
        0, version_resp_raw.length() - sizeof(uint16_t));
    VLOG(1) << "version " << *version_out;

    if (*version_out != kSupportedU2fVersion) {
      LOG(WARNING) << "Unsupported U2F version " << *version_out;
      return false;
    }
  }

  return !rc;
}

void U2fHid::ReturnError(U2fHidError errcode, uint32_t cid, bool clear) {
  HidMessage msg(U2fHidCommand::kError, cid);

  msg.AddByte(static_cast<uint8_t>(errcode));
  VLOG(1) << "ERROR/" << std::hex << static_cast<int>(errcode)
          << " CID:" << std::hex << cid;
  if (clear)
    transaction_ = std::make_unique<Transaction>();

  std::string report;
  msg.BuildReport(0, &report);
  hid_->SendReport(report);
}

void U2fHid::TransactionTimeout() {
  ReturnError(U2fHidError::kMsgTimeout, transaction_->cid, true);
}

void U2fHid::LockTimeout() {
  if (locked_cid_)
    LOG(WARNING) << "Cancelled lock CID:" << std::hex << locked_cid_;
  locked_cid_ = 0;
}

void U2fHid::ReturnResponse(const std::string& resp) {
  HidMessage msg(transaction_->cmd, transaction_->cid);
  int offset = 0;

  msg.AddPayload(resp);
  // Send all the chunks of the message (split in 64-B HID reports)
  do {
    std::string report;
    offset = msg.BuildReport(offset, &report);
    hid_->SendReport(report);
  } while (offset);
}

void U2fHid::ReturnFailureResponse(uint16_t sw) {
  std::string resp;
  U2fResponseAdpu resp_adpu;
  resp_adpu.SetStatus(sw);
  resp_adpu.ToString(&resp);

  ReturnResponse(resp);
}

void U2fHid::IgnorePowerButton() {
  // Duration of the user presence persistence on the firmware side
  const base::TimeDelta kPresenceTimeout = base::TimeDelta::FromSeconds(10);

  brillo::ErrorPtr err;
  // Mask the next power button press for the UI
  ignore_button_.Run(kPresenceTimeout.ToInternalValue(), &err, -1);
}

void U2fHid::MaybeIgnorePowerButton(const std::string& payload) {
  bool ignore = false;

  // The previous code assumes that U2F_REGISTER requests always require
  // physical presence, and this is the behavior described in the spec. However,
  // cr50 only requires physical presence for U2F_REGISTER (in both old and new
  // implementations) if the P1 value has the first bit set. Whether this bit is
  // set or not is not described in the spec.
  // TODO(louiscollard): Check if the above behavior is correct and update if
  // not.

  // All U2F_REGISTER requests require physical presence.
  // U2F_AUTHENTICATE requests may require physical presence.
  base::Optional<U2fCommandAdpu> adpu =
      U2fCommandAdpu::ParseFromString(payload);
  if (adpu.has_value()) {
    if (adpu->Ins() == U2fIns::kU2fRegister) {
      ignore = true;
    } else if (adpu->Ins() == U2fIns::kU2fAuthenticate) {
      base::Optional<U2fAuthenticateRequestAdpu> auth_adpu =
          U2fAuthenticateRequestAdpu::FromCommandAdpu(*adpu);
      ignore = auth_adpu.has_value() && auth_adpu->IsAuthenticateCheckOnly();
    }
  }

  if (ignore) {
    IgnorePowerButton();
  }
}

void U2fHid::CmdInit(uint32_t cid, const std::string& payload) {
  HidMessage msg(U2fHidCommand::kInit, cid);

  if (payload.size() != kInitNonceSize) {
    VLOG(1) << "Payload size " << payload.size();
    ReturnError(U2fHidError::kInvalidLen, cid, false);
    return;
  }

  VLOG(1) << "INIT CID:" << std::hex << cid << " NONCE "
          << base::HexEncode(payload.data(), payload.size());

  if (cid == kCidBroadcast) {  // Allocate Channel ID
    cid = free_cid_++;
    // Roll-over if needed
    if (free_cid_ == kCidBroadcast)
      free_cid_ = 1;
  }

  // Keep the nonce in the first 8 bytes
  msg.AddPayload(payload);

  std::string serial_cid(reinterpret_cast<char*>(&cid), sizeof(uint32_t));
  msg.AddPayload(serial_cid);

  // Append the versions : interface / major / minor / build
  msg.AddByte(kInterfaceVersion);
  msg.AddByte(0);
  msg.AddByte(0);
  msg.AddByte(0);
  // Append Capability flags
  // TODO(vpalatin) the Wink command is only outputting a trace for now,
  // do a real action or remove it.
  msg.AddByte(kCapFlagLock | kCapFlagWink);

  std::string report;
  msg.BuildReport(0, &report);
  hid_->SendReport(report);
}

int U2fHid::CmdPing(std::string* resp) {
  VLOG(1) << "PING len " << transaction_->total_size;

  // poke U2F version to simulate latency.
  std::string version;
  GetU2fVersion(&version);

  // send back the same content
  *resp = transaction_->payload.substr(0, transaction_->total_size);
  return transaction_->total_size;
}

int U2fHid::CmdLock(std::string* resp) {
  int duration = transaction_->payload[0];

  VLOG(1) << "LOCK " << duration << "s CID:" << std::hex << transaction_->cid;

  if (duration > kMaxLockDurationSeconds) {
    ReturnError(U2fHidError::kInvalidPar, transaction_->cid, true);
    return -EINVAL;
  }

  if (!duration) {
    lock_timeout_.Stop();
    locked_cid_ = 0;
  } else {
    locked_cid_ = transaction_->cid;
    lock_timeout_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(duration),
        base::Bind(&U2fHid::LockTimeout, base::Unretained(this)));
  }
  return 0;
}

int U2fHid::CmdWink(std::string* resp) {
  LOG(INFO) << "WINK!";
  return 0;
}

int U2fHid::CmdSysInfo(std::string* resp) {
  if (vendor_sysinfo_.empty()) {
    LOG(WARNING) << "No vendor system info available";
    ReturnError(U2fHidError::kInvalidCmd, transaction_->cid, true);
    return -EINVAL;
  }

  VLOG(1) << "SYSINFO len=" << vendor_sysinfo_.length();
  *resp = vendor_sysinfo_;
  return vendor_sysinfo_.length();
}

int U2fHid::CmdMsg(std::string* resp) {
  if (user_secrets_) {
    return ProcessMsg(resp);
  } else {
    return ForwardMsg(resp);
  }
}

int U2fHid::ForwardMsg(std::string* resp) {
  MaybeIgnorePowerButton(transaction_->payload);
  return transmit_apdu_.Run(transaction_->payload, resp);
}

int U2fHid::ProcessMsg(std::string* resp) {
  U2fIns ins = U2fIns::kInsInvalid;

  base::Optional<U2fCommandAdpu> adpu =
      U2fCommandAdpu::ParseFromString(transaction_->payload);
  if (adpu.has_value()) {
    ins = adpu->Ins();
  }

  // TODO(louiscollard): Check expected response length is large enough.

  switch (adpu->Ins()) {
    case U2fIns::kU2fRegister: {
      base::Optional<U2fRegisterRequestAdpu> reg_adpu =
          U2fRegisterRequestAdpu::FromCommandAdpu(*adpu);
      if (reg_adpu.has_value()) {
        return ProcessU2fRegister(*reg_adpu, resp);
      }
      break;  // Handle error.
    }
    case U2fIns::kU2fAuthenticate: {
      base::Optional<U2fAuthenticateRequestAdpu> auth_adpu =
          U2fAuthenticateRequestAdpu::FromCommandAdpu(*adpu);
      if (auth_adpu.has_value()) {
        return ProcessU2fAuthenticate(*auth_adpu, resp);
      }
      break;  // Handle error.
    }
    case U2fIns::kU2fVersion: {
      U2fResponseAdpu response;
      response.AppendString(kSupportedU2fVersion);
      response.SetStatus(U2F_SW_NO_ERROR);
      response.ToString(resp);
      return 0;
    }
    default:
      break;  // Handled below.
  }

  ReturnError(U2fHidError::kInvalidCmd, transaction_->cid, true);
  return -EINVAL;
}

namespace {

// Builds data to be signed as part of a U2F_REGISTER response, as defined by
// the "U2F Raw Message Formats" specification.
std::vector<uint8_t> BuildU2fRegisterResponseSignedData(
    const std::vector<uint8_t>& app_id,
    const std::vector<uint8_t>& challenge,
    const std::vector<uint8_t>& pub_key,
    const std::vector<uint8_t>& key_handle) {
  std::vector<uint8_t> signed_data;
  signed_data.push_back('\0');  // reserved byte
  util::AppendToVector(app_id, &signed_data);
  util::AppendToVector(challenge, &signed_data);
  util::AppendToVector(key_handle, &signed_data);
  util::AppendToVector(pub_key, &signed_data);
  return signed_data;
}

bool DoSoftwareAttest(const std::vector<uint8_t>& data_to_sign,
                      std::vector<uint8_t>* attestation_cert,
                      std::vector<uint8_t>* signature) {
  crypto::ScopedEC_KEY attestation_key = util::CreateAttestationKey();
  if (!attestation_key) {
    return false;
  }

  base::Optional<std::vector<uint8_t>> cert_result =
      util::CreateAttestationCertificate(attestation_key.get());
  base::Optional<std::vector<uint8_t>> attest_result =
      util::AttestToData(data_to_sign, attestation_key.get());

  if (!cert_result.has_value() || !attest_result.has_value()) {
    // These functions are never expected to fail.
    LOG(ERROR) << "U2F software attestation failed.";
    return false;
  }

  *attestation_cert = std::move(*cert_result);
  *signature = std::move(*attest_result);
  return true;
}

}  // namespace

int U2fHid::ProcessU2fRegister(U2fRegisterRequestAdpu request,
                               std::string* resp) {
  std::vector<uint8_t> pub_key;
  std::vector<uint8_t> key_handle;

  IgnorePowerButton();

  if (DoU2fGenerate(request.GetAppId(), &pub_key, &key_handle) !=
      kVendorCmdRcSuccess) {
    return -EINVAL;
  }

  std::vector<uint8_t> data_to_sign = BuildU2fRegisterResponseSignedData(
      request.GetAppId(), request.GetChallenge(), pub_key, key_handle);

  std::vector<uint8_t> attestation_cert;
  std::vector<uint8_t> signature;

  if (g2f_mode_ && request.UseG2fAttestation()) {
    attestation_cert = GetG2fCert();
    if (DoG2fAttest(data_to_sign, U2F_ATTEST_FORMAT_REG_RESP, &signature) !=
        kVendorCmdRcSuccess) {
      return -EINVAL;
    }
  } else {
    if (!DoSoftwareAttest(data_to_sign, &attestation_cert, &signature)) {
      ReturnError(U2fHidError::kOther, transaction_->cid, true);
      return -EINVAL;
    }
  }

  // Prepare response, as specified by "U2F Raw Message Formats".
  U2fResponseAdpu register_resp;
  register_resp.AppendByte(5 /* U2F_VER_2 */);
  register_resp.AppendBytes(pub_key);
  register_resp.AppendByte(key_handle.size());
  register_resp.AppendBytes(key_handle);
  register_resp.AppendBytes(attestation_cert);
  register_resp.AppendBytes(signature);
  register_resp.SetStatus(U2F_SW_NO_ERROR);
  register_resp.ToString(resp);

  return 0;
}

namespace {

// A success response to a U2F_AUTHENTICATE request includes a signature over
// the following data, in this format.
std::vector<uint8_t> BuildU2fAuthenticateResponseSignedData(
    const std::vector<uint8_t>& app_id,
    const std::vector<uint8_t>& challenge,
    const std::vector<uint8_t>& counter) {
  std::vector<uint8_t> to_sign;
  util::AppendToVector(app_id, &to_sign);
  to_sign.push_back(0x01 /* User Presence Verified */);
  util::AppendToVector(counter, &to_sign);
  util::AppendToVector(challenge, &to_sign);
  return to_sign;
}

}  // namespace

int U2fHid::ProcessU2fAuthenticate(U2fAuthenticateRequestAdpu request,
                                   std::string* resp) {
  if (!request.IsAuthenticateCheckOnly()) {
    IgnorePowerButton();
  }

  // This will increment the counter even if this request ends up failing due to
  // lack of presence of an invalid keyhandle.
  // TODO(louiscollard): Check this is ok.
  base::Optional<std::vector<uint8_t>> counter = user_state_->GetCounter();
  if (!counter.has_value()) {
    return -EINVAL;
  }

  std::vector<uint8_t> to_sign = BuildU2fAuthenticateResponseSignedData(
      request.GetAppId(), request.GetChallenge(), *counter);

  std::vector<uint8_t> signature;
  if (DoU2fSign(request.GetAppId(), request.GetKeyHandle(),
                util::Sha256(to_sign), &signature)) {
    return -EINVAL;
  }

  // Prepare response, as specified by "U2F Raw Message Formats".
  U2fResponseAdpu auth_resp;
  auth_resp.AppendByte(1 /* U2F_VER_2 */);
  auth_resp.AppendBytes(*counter);
  auth_resp.AppendBytes(signature);
  auth_resp.SetStatus(U2F_SW_NO_ERROR);
  auth_resp.ToString(resp);

  return 0;
}

int U2fHid::DoU2fGenerate(const std::vector<uint8_t>& app_id,
                          std::vector<uint8_t>* pub_key,
                          std::vector<uint8_t>* key_handle) {
  base::Optional<brillo::SecureBlob> user_secret = user_state_->GetUserSecret();
  if (!user_secret.has_value()) {
    ReturnError(U2fHidError::kOther, transaction_->cid, true);
    return -EINVAL;
  }

  U2F_GENERATE_REQ generate_req = {
      .flags = U2F_AUTH_FLAG_TUP  // Require user presence
  };
  util::VectorToObject(app_id, &generate_req.appId);
  util::VectorToObject(*user_secret, &generate_req.userSecret);

  U2F_GENERATE_RESP generate_resp = {};
  uint32_t generate_status = tpm_generate_.Run(generate_req, &generate_resp);

  if (generate_status != kVendorCmdRcSuccess) {
    VLOG(3) << "U2F_GENERATE failed, status: " << std::hex << generate_status;
    if (generate_status == kVendorCmdRcNotAllowed) {
      // We could not assert physical presence.
      ReturnFailureResponse(U2F_SW_CONDITIONS_NOT_SATISFIED);
    } else {
      // We sent an invalid request (u2fd programming error),
      // or internal cr50 error.
      ReturnError(U2fHidError::kOther, transaction_->cid, true);
    }
    return -EINVAL;
  }

  util::AppendToVector(generate_resp.pubKey, pub_key);
  util::AppendToVector(generate_resp.keyHandle, key_handle);

  return 0;
}

int U2fHid::DoU2fSign(const std::vector<uint8_t>& app_id,
                      const std::vector<uint8_t>& key_handle,
                      const std::vector<uint8_t>& hash,
                      std::vector<uint8_t>* signature_out) {
  base::Optional<brillo::SecureBlob> user_secret = user_state_->GetUserSecret();
  if (!user_secret.has_value()) {
    ReturnError(U2fHidError::kOther, transaction_->cid, true);
    return -EINVAL;
  }

  U2F_SIGN_REQ sign_req = {
      // TODO(louiscollard): Copy G2F_CONSUME to flags.
  };
  util::VectorToObject(app_id, sign_req.appId);
  util::VectorToObject(*user_secret, sign_req.userSecret);
  util::VectorToObject(key_handle, sign_req.keyHandle);
  util::VectorToObject(hash, sign_req.hash);

  U2F_SIGN_RESP sign_resp = {};
  uint32_t sign_status = tpm_sign_.Run(sign_req, &sign_resp);

  if (sign_status != kVendorCmdRcSuccess) {
    VLOG(3) << "U2F_SIGN failed, status: " << std::hex << sign_status;
    if (sign_status == kVendorCmdRcPasswordRequired) {
      // We have specified an invalid key handle.
      ReturnFailureResponse(U2F_SW_WRONG_DATA);
    } else if (sign_status == kVendorCmdRcNotAllowed) {
      // Could not assert user presence.
      ReturnFailureResponse(U2F_SW_CONDITIONS_NOT_SATISFIED);
    } else {
      // We sent an invalid request (u2fd programming error),
      // or internal cr50 error.
      ReturnError(U2fHidError::kOther, transaction_->cid, true);
    }
    return -EINVAL;
  }

  base::Optional<std::vector<uint8_t>> signature =
      util::SignatureToDerBytes(sign_resp.sig_r, sign_resp.sig_s);

  if (!signature.has_value()) {
    ReturnError(U2fHidError::kOther, transaction_->cid, true);
    return -EINVAL;
  }

  *signature_out = *signature;

  return 0;
}

int U2fHid::DoG2fAttest(const std::vector<uint8_t>& data,
                        uint8_t format,
                        std::vector<uint8_t>* signature_out) {
  base::Optional<brillo::SecureBlob> user_secret = user_state_->GetUserSecret();
  if (!user_secret.has_value()) {
    ReturnError(U2fHidError::kOther, transaction_->cid, true);
    return -EINVAL;
  }

  U2F_ATTEST_REQ attest_req = {.format = format,
                               .dataLen = static_cast<uint8_t>(data.size())};
  util::VectorToObject(*user_secret, attest_req.userSecret);
  // Only a programming error can cause this CHECK to fail.
  CHECK_LE(data.size(), sizeof(attest_req.data));
  util::VectorToObject(data, attest_req.data);

  U2F_ATTEST_RESP attest_resp = {};
  uint32_t attest_status = tpm_attest_.Run(attest_req, &attest_resp);

  if (attest_status != kVendorCmdRcSuccess) {
    LOG(ERROR) << "U2F_ATTEST failed, status: " << std::hex << attest_status;
    // We are attesting to a key handle that we just created, so if
    // attestation fails we have hit some internal error.
    ReturnError(U2fHidError::kOther, transaction_->cid, true);
    return -EINVAL;
  }

  base::Optional<std::vector<uint8_t>> signature =
      util::SignatureToDerBytes(attest_resp.sig_r, attest_resp.sig_s);

  if (!signature.has_value()) {
    LOG(ERROR) << "DER encoding of U2F_ATTEST signature failed.";
    ReturnError(U2fHidError::kOther, transaction_->cid, true);
    return -EINVAL;
  }

  *signature_out = *signature;

  return 0;
}

const std::vector<uint8_t>& U2fHid::GetG2fCert() {
  static std::vector<uint8_t> cert = [this]() {
    std::string cert_str;
    std::vector<uint8_t> cert_tmp;

    uint32_t get_cert_status = tpm_g2f_cert_.Run(&cert_str);
    util::AppendToVector(cert_str, &cert_tmp);

    if (get_cert_status != kVendorCmdRcSuccess ||
        !util::RemoveCertificatePadding(&cert_tmp)) {
      // TODO(louiscollard): Retry?
      LOG(ERROR) << "Failed to retrieve G2F certificate, status: " << std::hex
                 << get_cert_status;
      cert_tmp.clear();
    }

    return cert_tmp;
  }();
  return cert;
}

void U2fHid::ExecuteCmd() {
  int rc;
  std::string resp;

  transaction_->timeout.Stop();
  switch (transaction_->cmd) {
    case U2fHidCommand::kMsg:
      rc = CmdMsg(&resp);
      break;
    case U2fHidCommand::kPing:
      rc = CmdPing(&resp);
      break;
    case U2fHidCommand::kLock:
      rc = CmdLock(&resp);
      break;
    case U2fHidCommand::kWink:
      rc = CmdWink(&resp);
      break;
    case U2fHidCommand::kVendorSysInfo:
      rc = CmdSysInfo(&resp);
      break;
    default:
      LOG(WARNING) << "Unknown command " << std::hex
                   << static_cast<int>(transaction_->cmd);
      ReturnError(U2fHidError::kInvalidCmd, transaction_->cid, true);
      return;
  }

  if (rc >= 0)
    ReturnResponse(resp);

  // we are done with this transaction
  transaction_ = std::make_unique<Transaction>();
}

void U2fHid::ProcessReport(const std::string& report) {
  HidPacket pkt(report);

  VLOG(2) << "RX RPT/" << report.size() << " ["
          << base::HexEncode(report.data(), report.size()) << "]";
  if (!pkt.IsValidFrame())
    return;  // Invalid report

  // Check frame validity
  if (pkt.ChannelId() == 0) {
    VLOG(1) << "No frame should use channel 0";
    ReturnError(U2fHidError::kInvalidCid,
                pkt.ChannelId(),
                pkt.ChannelId() == transaction_->cid);
    return;
  }

  if (pkt.IsInitFrame() && pkt.Command() == U2fHidCommand::kInit) {
    if (pkt.ChannelId() == transaction_->cid) {
      // Abort an ongoing multi-packet transaction
      VLOG(1) << "Transaction cancelled on CID:" << std::hex << pkt.ChannelId();
      transaction_ = std::make_unique<Transaction>();
    }
    // special case: INIT should not interrupt other commands
    CmdInit(pkt.ChannelId(), report.substr(pkt.PayloadIndex(), kInitNonceSize));
    return;
  }
  // not an INIT command from here

  if (pkt.IsInitFrame()) {  // INIT frame type (not the INIT command)
    if (pkt.ChannelId() == kCidBroadcast) {
      VLOG(1) << "INIT command not on broadcast CID:" << std::hex
              << pkt.ChannelId();
      ReturnError(U2fHidError::kInvalidCid, pkt.ChannelId(), false);
      return;
    }
    if (locked_cid_ && pkt.ChannelId() != locked_cid_) {
      // somebody else has the lock
      VLOG(1) << "channel locked by CID:" << std::hex << locked_cid_;
      ReturnError(U2fHidError::kChannelBusy, pkt.ChannelId(), false);
      return;
    }
    if (transaction_->cid && (pkt.ChannelId() != transaction_->cid)) {
      VLOG(1) << "channel used by CID:" << std::hex << transaction_->cid;
      ReturnError(U2fHidError::kChannelBusy, pkt.ChannelId(), false);
      return;
    }
    if (transaction_->cid) {
      VLOG(1) << "CONT frame expected";
      ReturnError(U2fHidError::kInvalidSeq, pkt.ChannelId(), true);
      return;
    }
    if (pkt.MessagePayloadSize() > kMaxPayloadSize) {
      VLOG(1) << "Invalid length " << pkt.MessagePayloadSize();
      ReturnError(U2fHidError::kInvalidLen, pkt.ChannelId(), true);
      return;
    }

    transaction_->timeout.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kU2fHidTimeoutMs),
        base::Bind(&U2fHid::TransactionTimeout, base::Unretained(this)));

    // record transaction parameters
    transaction_->cid = pkt.ChannelId();
    transaction_->total_size = pkt.MessagePayloadSize();
    transaction_->cmd = pkt.Command();
    transaction_->seq = 0;
    transaction_->payload =
        report.substr(pkt.PayloadIndex(), transaction_->total_size);
  } else {  // CONT Frame
    if (transaction_->cid == 0 || transaction_->cid != pkt.ChannelId()) {
      VLOG(1) << "invalid CONT";
      return;  // just ignore
    }
    if (transaction_->seq != pkt.SeqNumber()) {
      VLOG(1) << "invalid sequence " << static_cast<int>(pkt.SeqNumber())
              << " !=  " << transaction_->seq;
      ReturnError(U2fHidError::kInvalidSeq,
                  pkt.ChannelId(),
                  pkt.ChannelId() == transaction_->cid);
      return;
    }
    // reload timeout
    transaction_->timeout.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kU2fHidTimeoutMs),
        base::Bind(&U2fHid::TransactionTimeout, base::Unretained(this)));
    // record the payload
    transaction_->payload += report.substr(pkt.PayloadIndex());
    transaction_->seq++;
  }
  // Are we done with this transaction ?
  if (transaction_->payload.size() >= transaction_->total_size)
    ExecuteCmd();
}

}  // namespace u2f
