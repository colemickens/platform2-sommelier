// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_NSS_UTIL_H_
#define LOGIN_MANAGER_NSS_UTIL_H_

#include <base/basictypes.h>
#include <string>
#include <vector>

class FilePath;

namespace base {
class RSAPrivateKey;
}

namespace login_manager {

// An interface to wrap the usage of base/nss_util.h and allow for mocking.
class NssUtil {
 public:
  class Factory {
   public:
    virtual NssUtil* CreateNssUtil() = 0;
  };

  NssUtil();
  virtual ~NssUtil();

  // Sets the factory used by the static method Create to create an
  // NssUtil.  NssUtil does not take ownership of
  // |factory|. A value of NULL results in an NssUtil being
  // created directly.
#if defined(UNIT_TEST)
  static void set_factory(Factory* factory) { factory_ = factory; }
#endif

  // Creates an NssUtil, ownership returns to the caller. If there is no
  // Factory (the default) this creates and returns a new NssUtil.
  static NssUtil* Create();

  static void KeyFromBuffer(const std::string& buf, std::vector<uint8>* out);

  virtual bool OpenUserDB() = 0;

  // Caller takes ownership of returned key.
  virtual base::RSAPrivateKey* GetPrivateKey(
      const std::vector<uint8>& public_key_der) = 0;

  // Caller takes ownership of returned key.
  virtual base::RSAPrivateKey* GenerateKeyPair() = 0;

  virtual FilePath GetOwnerKeyFilePath() = 0;

  virtual bool Verify(const uint8* algorithm, int algorithm_len,
                      const uint8* signature, int signature_len,
                      const uint8* data, int data_len,
                      const uint8* public_key, int public_key_len) = 0;

  virtual bool Sign(const uint8* data, int data_len,
                    std::vector<uint8>* OUT_signature,
                    base::RSAPrivateKey* key) = 0;

 private:
  static Factory* factory_;
  DISALLOW_COPY_AND_ASSIGN(NssUtil);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_NSS_UTIL_H_
