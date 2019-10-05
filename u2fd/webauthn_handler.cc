// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "u2fd/webauthn_handler.h"

#include <u2f/proto_bindings/u2f_interface.pb.h>

namespace u2f {

MakeCredentialResponse WebAuthnHandler::MakeCredential(
    const MakeCredentialRequest& request) {
  // TODO(louiscollard): Implement.
  return MakeCredentialResponse();
}

GetAssertionResponse WebAuthnHandler::GetAssertion(
    const GetAssertionRequest& request) {
  // TODO(louiscollard): Implement.
  return GetAssertionResponse();
}

HasCredentialsResponse WebAuthnHandler::HasCredentials(
    const HasCredentialsRequest& request) {
  // TODO(louiscollard): Implement.
  return HasCredentialsResponse();
}

}  // namespace u2f
