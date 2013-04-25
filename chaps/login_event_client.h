// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_LOGIN_EVENT_CLIENT_H_
#define CHAPS_LOGIN_EVENT_CLIENT_H_

#include <string>
#include <vector>

#include <base/basictypes.h>

#include "pkcs11/cryptoki.h"
#include "chromeos/secure_blob.h"
#include "chaps/chaps.h"

namespace chaps {

class ChapsProxyImpl;

// Sends login events to the Chaps daemon. Example usage:
//   LoginEventClient client;
//   if (!client.Init())
//     ...
//   client.OpenIsolate(...);
//   client.FireLoadTokenEvent(...);
class EXPORT_SPEC LoginEventClient {
 public:
  LoginEventClient();
  virtual ~LoginEventClient();

  // Open an isolate into which tokens can be loaded. To attempt to open an
  // existing isolate, pass its isolate credential, otherwise pass be empty
  // SecureBlob to create a new isolate.  Returns true if successful.
  //
  //  isolate_credential - The users isolate into which to login, or a empty if
  //                 logging in to a new isolate. On return contains the isolate
  //                 credential for the isolate the user is logged in on.
  //  new_isolate_created - Returns true if a new isolate was created (in which
  //                        case isolate_credential will be set to the new
  //                        isolate's credential), or false if the call
  //                        succeeded in opening the existing isolate.
  virtual bool OpenIsolate(chromeos::SecureBlob* isolate_credential,
                           bool* new_isolate_created);

  // Close a given isolate. If all outstanding OpenIsolate calls have been
  // closed, then all tokens will be unloaded from the isolate and the isolate
  // will be destroyed.
  //  isolate_credential - The isolate into which they are logging out from.
  virtual void CloseIsolate(const chromeos::SecureBlob& isolate_credential);

  // Sends a load token event. The Chaps daemon will insert a token into the
  // given user's isolate.  Returns true on success.
  //  isolate_credential - The isolate into which the token should be loaded.
  //  path - The path to the user's token directory.
  //  auth_data - Authorization data to unlock the token.
  //  slot_id - On success, will be set to the loaded token's slot ID.
  virtual bool LoadToken(const chromeos::SecureBlob& isolate_credential,
                         const std::string& path,
                         const chromeos::SecureBlob& auth_data,
                         int* slot_id);

  // Sends an unload event. The Chaps daemon will remove the token from the
  // given users isolate.
  //  isolate_credential - The isolate into which the token should be unloaded.
  //  path - The path to the user's token directory.
  virtual void UnloadToken(const chromeos::SecureBlob& isolate_credential,
                           const std::string& path);

  // Notifies Chaps that a token's authorization data has been changed. The
  // Chaps daemon will re-protect the token with the new data.
  //  path - The path to the user's token directory.
  //  old_auth_data - Authorization data to unlock the token as it is.
  //  new_auth_data - The new authorization data.
  virtual void ChangeTokenAuthData(const std::string& path,
                                   const chromeos::SecureBlob& old_auth_data,
                                   const chromeos::SecureBlob& new_auth_data);
 private:
  ChapsProxyImpl* proxy_;
  bool is_connected_;

  // Attempts to connect to the Chaps daemon. Returns true on success.
  bool Connect();

  DISALLOW_COPY_AND_ASSIGN(LoginEventClient);
};

}  // namespace chaps

#endif  // CHAPS_LOGIN_EVENT_CLIENT_H_
