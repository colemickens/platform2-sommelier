// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef U2FD_WEBAUTHN_HANDLER_H_
#define U2FD_WEBAUTHN_HANDLER_H_

#include <u2f/proto_bindings/u2f_interface.pb.h>

namespace u2f {

// Implementation of the WebAuthn DBus API.
// More detailed documentation is available in u2f_interface.proto
class WebAuthnHandler {
 public:
  // Generates a new credential.
  MakeCredentialResponse MakeCredential(const MakeCredentialRequest& request);

  // Signs a challenge from the relaying party.
  GetAssertionResponse GetAssertion(const GetAssertionRequest& request);

  // Tests validity and/or presence of specified credentials.
  HasCredentialsResponse HasCredentials(const HasCredentialsRequest& request);
};

}  // namespace u2f

#endif  // U2FD_WEBAUTHN_HANDLER_H_
