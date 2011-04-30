// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_NSS_UTIL_H_
#define LOGIN_MANAGER_MOCK_NSS_UTIL_H_

#include "login_manager/nss_util.h"

#include <unistd.h>
#include <base/file_path.h>
#include <base/nss_util.h>
#include <gmock/gmock.h>

namespace base {
class RSAPrivateKey;
}

namespace login_manager {

class MockNssUtil : public NssUtil {
 public:
  virtual ~MockNssUtil();

  MOCK_METHOD0(MightHaveKeys, bool());
  MOCK_METHOD0(OpenUserDB, bool());
  MOCK_METHOD1(GetPrivateKey, base::RSAPrivateKey*(const std::vector<uint8>&));
  MOCK_METHOD0(GenerateKeyPair, base::RSAPrivateKey*());
  MOCK_METHOD0(GetOwnerKeyFilePath, FilePath());
  MOCK_METHOD8(Verify, bool(const uint8* algorithm, int algorithm_len,
                            const uint8* signature, int signature_len,
                            const uint8* data, int data_len,
                            const uint8* public_key, int public_key_len));
  MOCK_METHOD4(Sign, bool(const uint8* data, int data_len,
                          std::vector<uint8>* OUT_signature,
                          base::RSAPrivateKey* key));
 protected:
  MockNssUtil();
  void ExpectGetOwnerKeyFilePath();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNssUtil);
};

template<typename T>
class MockFactory : public NssUtil::Factory {
 public:
  MockFactory() {}
  ~MockFactory() {}
  NssUtil* CreateNssUtil() {
    return new T;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(MockFactory);
};

class KeyCheckUtil : public MockNssUtil {
 public:
  KeyCheckUtil();
  virtual ~KeyCheckUtil();
 private:
  DISALLOW_COPY_AND_ASSIGN(KeyCheckUtil);
};

class KeyFailUtil : public MockNssUtil {
 public:
  KeyFailUtil();
  virtual ~KeyFailUtil();
 private:
  DISALLOW_COPY_AND_ASSIGN(KeyFailUtil);
};

class SadNssUtil : public MockNssUtil {
 public:
  SadNssUtil();
  virtual ~SadNssUtil();
 private:
  DISALLOW_COPY_AND_ASSIGN(SadNssUtil);
};

class EmptyNssUtil : public MockNssUtil {
 public:
  EmptyNssUtil();
  virtual ~EmptyNssUtil();
 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyNssUtil);
};

class ShortKeyGenerator : public MockNssUtil {
 public:
  ShortKeyGenerator();
  virtual ~ShortKeyGenerator();
  static base::RSAPrivateKey* CreateFake();

 private:
  DISALLOW_COPY_AND_ASSIGN(ShortKeyGenerator);
};

class ShortKeyUtil : public ShortKeyGenerator {
 public:
  ShortKeyUtil();
  virtual ~ShortKeyUtil();
 private:
  DISALLOW_COPY_AND_ASSIGN(ShortKeyUtil);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_NSS_UTIL_H_
