// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef U2FD_U2FHID_H_
#define U2FD_U2FHID_H_

#include <memory>
#include <string>
#include <vector>

#include <base/timer/timer.h>
#include <brillo/errors/error.h>
#include <trunks/cr50_headers/u2f.h>

#include "u2fd/hid_interface.h"
#include "u2fd/u2f_adpu.h"

namespace u2f {

constexpr uint32_t kDefaultVendorId = 0x18d1;
constexpr uint32_t kDefaultProductId = 0x502c;

// Mandatory length of the U2F HID report.
constexpr size_t kU2fReportSize = 64;

// HID frame CMD/SEQ byte definitions.
constexpr uint8_t kFrameTypeMask = 0x80;
constexpr uint8_t kFrameTypeInit = 0x80;
// when bit 7 is not set, the frame type is CONTinuation.

// INIT command parameters
constexpr uint32_t kCidBroadcast = -1U;
constexpr size_t kInitNonceSize = 8;

constexpr uint8_t kCapFlagWink = 0x01;
constexpr uint8_t kCapFlagLock = 0x02;

constexpr size_t kMaxPayloadSize = (64 - 7 + 128 * (64 - 5));  // 7609 bytes

// U2fHid emulates U2FHID protocol on top of the TPM U2F implementation.
// The object reads the HID report sent by the HIDInterface passed to the
// constructor, parses it and extracts the U2FHID command. If this is a U2F
// message, finally sends the raw U2F APDU to the |transmit_func| callback
// passed to the constructor. It returns the final result (response APDU or
// error code) inside an HID report through the HIDInterface.
class U2fHid {
 public:
  // U2FHID Command codes
  enum class U2fHidCommand : uint8_t {
    kPing = 1,
    kMsg = 3,
    kLock = 4,
    kVendorSysInfo = 5,
    kInit = 6,
    kWink = 8,
    kError = 0x3f,
  };

  // U2FHID error codes
  enum class U2fHidError : uint8_t {
    kNone = 0,
    kInvalidCmd = 1,
    kInvalidPar = 2,
    kInvalidLen = 3,
    kInvalidSeq = 4,
    kMsgTimeout = 5,
    kChannelBusy = 6,
    kLockRequired = 10,
    kInvalidCid = 11,
    kOther = 127,
  };

  // Callback to send the raw U2F APDU in |req| and get the corresponding
  // response APDU in |resp|.
  using TpmAdpuCallback =
      base::Callback<uint32_t(const std::string& req, std::string* resp)>;
  // Callback to run the VENDOR_CC_U2F_GENERATE command.
  using TpmGenerateCallback = base::Callback<uint32_t(
      const U2F_GENERATE_REQ& req, U2F_GENERATE_RESP* resp)>;
  // Callback to run the VENDOR_CC_U2F_SIGN command.
  using TpmSignCallback =
      base::Callback<uint32_t(const U2F_SIGN_REQ& req, U2F_SIGN_RESP* resp)>;
  // Callback to run the VENDOR_CC_U2F_ATTEST command.
  using TpmAttestCallback = base::Callback<uint32_t(const U2F_ATTEST_REQ& req,
                                                    U2F_ATTEST_RESP* resp)>;
  // Callback to retrieve the G2F certificate.
  using TpmG2fCertCallback = base::Callback<uint32_t(std::string* cert_out)>;
  // Callback to disable the power button for |in_timeout_internal| when using
  // it as physical presence for U2F.
  using IgnoreButtonCallback = base::Callback<bool(
      int64_t in_timeout_internal, brillo::ErrorPtr* error, int timeout)>;

  // TODO(louiscollard): Pass TpmVendorCommandProxy rather than individual
  // callbacks.
  U2fHid(std::unique_ptr<HidInterface> hid,
         const std::string& vendor_sysinfo,
         const bool g2f_mode,
         const bool user_secrets,
         const TpmAdpuCallback& adpu_fn,
         const TpmGenerateCallback& generate_fn,
         const TpmSignCallback& sign_fn,
         const TpmAttestCallback& attest_fn,
         const TpmG2fCertCallback& g2f_cert_fn,
         const IgnoreButtonCallback& ignore_button_func);
  ~U2fHid();
  bool Init();

  // Retrieves the U2f implementation version available through the
  // |transmit_func| provided to the constructor.
  // Returns true on success.
  bool GetU2fVersion(std::string* version_out);

 private:
  // U2FHID protocol commands implementation.
  void CmdInit(uint32_t cid, const std::string& payload);
  int CmdLock(std::string* resp);
  int CmdMsg(std::string* resp);
  int CmdPing(std::string* resp);
  int CmdSysInfo(std::string* resp);
  int CmdWink(std::string* resp);

  // Fully resets the state of the possibly on-going U2FHID transaction.
  void ClearTransaction();

  // Sends back a U2FHID report with just the |errcode| error code inside
  // on channel |cid|.
  // If |clear| is set, clear the transaction state at the same time.
  void ReturnError(U2fHidError errcode, uint32_t cid, bool clear);

  // Called when we reach the deadline for the on-going transaction.
  void TransactionTimeout();

  // Called when we reach the deadline for an unreleased channel lock.
  void LockTimeout();

  // Sends back a U2FHID report indicating success and carrying the response
  // payload |resp|.
  void ReturnResponse(const std::string& resp);

  // Sends back a U2FHID report indicating failure and carrying a response
  // code.
  void ReturnFailureResponse(uint16_t sw);

  // Ignores power button presses for 10 seconds.
  void IgnorePowerButton();

  // Parses the ISO7816-4:2005 U2F APDU contained in |payload| to guess
  // if it contains a user physical presence request and mask the power button
  // actions if it does.
  void MaybeIgnorePowerButton(const std::string& payload);

  // Executes the action requested by the command contained in the current
  // transaction.
  void ExecuteCmd();

  // Parses the HID report contained in |report| and append the content to the
  // current U2FHID transaction or create a new one.
  void ProcessReport(const std::string& report);

  // U2F message handler implementations.
  //////

  // Forwards the U2F ADPU to cr50 as-is. This makes use of deprecated cr50
  // functionality.
  int ForwardMsg(std::string* resp);

  // Processes the ADPU and builds a response locally, making using of cr50
  // vendor commands where necessary.
  int ProcessMsg(std::string* resp);

  // Process a U2F_REGISTER ADPU.
  int ProcessU2fRegister(U2fRegisterRequestAdpu request, std::string* resp);
  // Process a U2F_AUTHENTICATE ADPU.
  int ProcessU2fAuthenticate(U2fAuthenticateRequestAdpu request,
                             std::string* resp);

  // Wrapper functions for cr50 U2F vendor commands.
  //////

  // Run a U2F_GENERATE command to create a new key handle.
  int DoU2fGenerate(const std::vector<uint8_t>& app_id,
                    std::vector<uint8_t>* pub_key,
                    std::vector<uint8_t>* key_handle);
  // Run a U2F_SIGN command to sign a hash using an existing key handle.
  int DoU2fSign(const std::vector<uint8_t>& app_id,
                const std::vector<uint8_t>& key_handle,
                const std::vector<uint8_t>& hash,
                std::vector<uint8_t>* signature);
  // Run a U2F_ATTEST command to sign data using the cr50 individual attestation
  // certificate.
  int DoG2fAttest(const std::vector<uint8_t>& data,
                  uint8_t format,
                  std::vector<uint8_t>* sig_out);
  // Returns the cr50 individual attestation certificate.
  const std::vector<uint8_t>& GetG2fCert();

  // U2F local state functions.
  //////

  // Returns the user secret for the current user.
  const std::vector<uint8_t> GetUserSecret() {
    // TODO(louiscollard): Implement.
    return std::vector<uint8_t>(32);
  }

  // Returns the current global counter value.
  std::vector<uint8_t> GetCounter() {
    // TODO(louiscollard): Implement.
    return {0, 0, 0, 0};
  }

  std::unique_ptr<HidInterface> hid_;
  const bool g2f_mode_;
  const bool user_secrets_;
  TpmAdpuCallback transmit_apdu_;
  TpmGenerateCallback tpm_generate_;
  TpmSignCallback tpm_sign_;
  TpmAttestCallback tpm_attest_;
  TpmG2fCertCallback tpm_g2f_cert_;
  IgnoreButtonCallback ignore_button_;
  uint32_t free_cid_;
  uint32_t locked_cid_;
  base::OneShotTimer lock_timeout_;

  std::string vendor_sysinfo_;

  class HidPacket;
  class HidMessage;
  struct Transaction;

  std::unique_ptr<Transaction> transaction_;

  DISALLOW_COPY_AND_ASSIGN(U2fHid);
};

}  // namespace u2f

#endif  // U2FD_U2FHID_H_
