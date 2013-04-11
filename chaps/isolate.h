// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_CHAPS_ISOLATE_H_
#define CHAPS_CHAPS_ISOLATE_H_

#include <string>

#include <chromeos/secure_blob.h>

#include "chaps/chaps.h"

namespace chaps {

const size_t kIsolateCredentialBytes = 16;

class IsolateCredentialManager {

 public:
  IsolateCredentialManager();
  virtual ~IsolateCredentialManager();

  // Get the well known credential for the default isolate.
  static chromeos::SecureBlob GetDefaultIsolateCredential() {
    // Default isolate credential is all zeros.
    return chromeos::SecureBlob(kIsolateCredentialBytes);
  }

  // Get the isolate credential for the current user, returning true if it
  // exists.
  virtual bool GetCurrentUserIsolateCredential(
      chromeos::SecureBlob* isolate_credential);

  // Get the isolate credential for the given user name, returning true if it
  // exists.
  virtual bool GetUserIsolateCredential(
      const std::string& user,
      chromeos::SecureBlob* isolate_credential);

 private:
  DISALLOW_COPY_AND_ASSIGN(IsolateCredentialManager);
};

}  // namespace chaps

#endif  // CHAPS_CHAPS_ISOLATE_H_
