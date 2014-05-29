// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_AUTHORIZATION_DELEGATE_H_
#define TRUNKS_AUTHORIZATION_DELEGATE_H_

#include <string>

namespace Trunks {

// AuthorizationDelegate is an interface passed to TPM commands. The delegate
// takes care of providing the authorization data for commands and verifying
// authorization data for responses. It also handles parameter encryption for
// commands and parameter decryption for responses.
class AuthorizationDelegate {
 public:
  // Provides authorization data for a command which has a cpHash value of
  // |command_hash|. On success, |authorization| is populated with the exact
  // octets for the Authorization Area of the command.  Returns true on success.
  virtual bool GetCommandAuthorization(const std::string& command_hash,
                                       std::string* authorization) = 0;

  // Checks authorization data for a response which has a rpHash value of
  // |response_hash|. The exact octets from the Authorization Area of the
  // response are given in |authorization|. Returns true iff the authorization
  // is valid.
  virtual bool CheckResponseAuthorization(const std::string& response_hash,
                                          const std::string& authorization) = 0;

  // Encrypts |parameters_in| if encryption is enabled. If encryption is not
  // enabled |parameters_out| is populated without encrypting. Returns true on
  // success.
  virtual bool EncryptCommandParameters(const std::string& parameters_in,
                                        std::string* parameters_out) = 0;

  // Decrypts |parameters_in| if encryption is enabled. If encryption is not
  // enabled |parameters_out| is populated without decrypting. Returns true on
  // success.
  virtual bool DecryptResponseParameters(const std::string& parameters_in,
                                         std::string* parameters_out) = 0;
};

}  // namespace Trunks

#endif  // TRUNKS_AUTHORIZATION_DELEGATE_H_
