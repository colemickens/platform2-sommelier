// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_TOKEN_MANAGER_CLIENT_H_
#define CHAPS_TOKEN_MANAGER_CLIENT_H_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>

#include "pkcs11/cryptoki.h"
#include "chromeos/secure_blob.h"
#include "chaps/chaps.h"
#include "chaps/token_manager_interface.h"

namespace chaps {

class ChapsProxyImpl;

// TokenManagerClient is an implementation of TokenManagerInterface which sends
// the token management calls to the Chaps daemon. Example usage:
//   TokenManagerClient client;
//   client.OpenIsolate(&my_isolate_credential, &new_isolate_created);
//   client.LoadToken(my_isolate_credential,
//                    "path/to/token",
//                    "1234",
//                    "MyTokenLabel",
//                    &slot_id);
class EXPORT_SPEC TokenManagerClient : public TokenManagerInterface {
 public:
  TokenManagerClient();
  virtual ~TokenManagerClient();

  // TokenManagerInterface methods.
  virtual bool OpenIsolate(chromeos::SecureBlob* isolate_credential,
                           bool* new_isolate_created);
  virtual void CloseIsolate(const chromeos::SecureBlob& isolate_credential);
  virtual bool LoadToken(const chromeos::SecureBlob& isolate_credential,
                         const FilePath& path,
                         const chromeos::SecureBlob& auth_data,
                         const std::string& label,
                         int* slot_id);
  virtual void UnloadToken(const chromeos::SecureBlob& isolate_credential,
                           const FilePath& path);
  virtual void ChangeTokenAuthData(const FilePath& path,
                                   const chromeos::SecureBlob& old_auth_data,
                                   const chromeos::SecureBlob& new_auth_data);
  virtual bool GetTokenPath(const chromeos::SecureBlob& isolate_credential,
                            int slot_id,
                            FilePath* path);

  // Convenience method, not on TokenManagerInterface.
  // Returns true on success, false on failure. If it succeeds, stores a list of
  // the paths of all loaded tokens in |results|.
  virtual bool GetTokenList(const chromeos::SecureBlob& isolate_credential,
                            std::vector<std::string>* results);

 private:
  scoped_ptr<ChapsProxyImpl> proxy_;
  bool is_connected_;

  // Attempts to connect to the Chaps daemon. Returns true on success.
  bool Connect();

  DISALLOW_COPY_AND_ASSIGN(TokenManagerClient);
};

}  // namespace chaps

#endif  // CHAPS_TOKEN_MANAGER_CLIENT_H_
