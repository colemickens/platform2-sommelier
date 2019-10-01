// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef U2FD_U2F_MSG_HANDLER_H_
#define U2FD_U2F_MSG_HANDLER_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <attestation/proto_bindings/interface.pb.h>
#include <base/optional.h>
#include <metrics/metrics_library.h>

#include "u2fd/allowlisting_util.h"
#include "u2fd/tpm_vendor_cmd.h"
#include "u2fd/u2f_adpu.h"
#include "u2fd/user_state.h"

namespace u2f {

// Processes incoming U2F messages, and produces corresponding responses.
class U2fMessageHandler {
 public:
  // Constructs a new message handler. Does not take ownership of proxy or
  // metrics, both of which must outlive this instance.
  U2fMessageHandler(std::unique_ptr<UserState> user_state,
                    std::unique_ptr<AllowlistingUtil> allowlisting_util,
                    std::function<void()> request_user_presence,
                    TpmVendorCommandProxy* proxy,
                    MetricsLibraryInterface* metrics,
                    bool allow_legacy_kh_sign,
                    bool allow_g2f_attestation);

  // Processes the ADPU and builds a response locally, making using of cr50
  // vendor commands where necessary.
  U2fResponseAdpu ProcessMsg(const std::string& request);

 private:
  // Process a U2F_REGISTER ADPU.
  U2fResponseAdpu ProcessU2fRegister(const U2fRegisterRequestAdpu& request);
  // Process a U2F_AUTHENTICATE ADPU.
  U2fResponseAdpu ProcessU2fAuthenticate(
      const U2fAuthenticateRequestAdpu& request);

  // Status for execution of a cr50 command. Includes status of preparation
  // of the request, actual execution of the cr50 command, and any processing
  // of the response.
  enum class Cr50CmdStatus : uint32_t {
    // Cr50 return codes, map to vendor_cmd_rc in tpm_vendor_cmds.h
    kSuccess = 0,
    kNotAllowed = 0x507,
    kPasswordRequired = 0x50a,
    // Errors that occur in u2fd while processing requests/responses.
    kInvalidState = 0x580,
    kInvalidResponseData,
  };

  // Wrapper functions for cr50 U2F vendor commands.

  // Run a U2F_GENERATE command to create a new key handle.
  Cr50CmdStatus DoU2fGenerate(const std::vector<uint8_t>& app_id,
                              std::vector<uint8_t>* pub_key,
                              std::vector<uint8_t>* key_handle);
  // Run a U2F_SIGN command to sign a hash using an existing key handle.
  Cr50CmdStatus DoU2fSign(const std::vector<uint8_t>& app_id,
                          const std::vector<uint8_t>& key_handle,
                          const std::vector<uint8_t>& hash,
                          std::vector<uint8_t>* signature);
  // Run a U2F_SIGN command to check if a key handle is valid.
  Cr50CmdStatus DoU2fSignCheckOnly(const std::vector<uint8_t>& app_id,
                                   const std::vector<uint8_t>& key_handle);
  // Run a U2F_ATTEST command to sign data using the cr50 individual attestation
  // certificate. Returns true on success.
  Cr50CmdStatus DoG2fAttest(const std::vector<uint8_t>& data,
                            uint8_t format,
                            std::vector<uint8_t>* sig_out);

  // Retrieves the G2F certificate from cr50. Returns nullopt on failure.
  base::Optional<std::vector<uint8_t>> GetG2fCert();

  // Builds an empty U2F response with the specified status code.
  U2fResponseAdpu BuildEmptyResponse(uint16_t sw);

  // Builds an empty U2F response with a U2F status code corresponding to the
  // specified cr50 status.
  U2fResponseAdpu BuildErrorResponse(Cr50CmdStatus status);

  std::unique_ptr<UserState> user_state_;
  std::unique_ptr<AllowlistingUtil> allowlisting_util_;
  std::function<void()> request_user_presence_;
  TpmVendorCommandProxy* proxy_;
  MetricsLibraryInterface* metrics_;

  const bool allow_legacy_kh_sign_;
  const bool allow_g2f_attestation_;
};

}  // namespace u2f

#endif  // U2FD_U2F_MSG_HANDLER_H_
