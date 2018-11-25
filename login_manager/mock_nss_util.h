// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_NSS_UTIL_H_
#define LOGIN_MANAGER_MOCK_NSS_UTIL_H_

#include "login_manager/nss_util.h"

#include <stdint.h>
#include <unistd.h>

#include <memory>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/scoped_temp_dir.h>
#include <crypto/nss_util.h>
#include <crypto/rsa_private_key.h>
#include <crypto/scoped_nss_types.h>
#include <crypto/scoped_test_nss_db.h>
#include <gmock/gmock.h>

namespace crypto {
class RSAPrivateKey;
}

namespace login_manager {
// Forward declaration.
typedef struct PK11SlotInfoStr PK11SlotInfo;

class MockNssUtil : public NssUtil {
 public:
  MockNssUtil();
  ~MockNssUtil() override;

  std::unique_ptr<crypto::RSAPrivateKey> CreateShortKey();

  crypto::ScopedPK11Slot OpenUserDB(
      const base::FilePath& user_homedir) override;
  MOCK_METHOD2(GetPrivateKeyForUser,
               std::unique_ptr<crypto::RSAPrivateKey>(
                   const std::vector<uint8_t>&, PK11SlotInfo*));
  MOCK_METHOD1(GenerateKeyPairForUser,
               std::unique_ptr<crypto::RSAPrivateKey>(PK11SlotInfo*));
  MOCK_METHOD0(GetNssdbSubpath, base::FilePath());
  MOCK_METHOD1(CheckPublicKeyBlob, bool(const std::vector<uint8_t>&));
  MOCK_METHOD3(Verify,
               bool(const std::vector<uint8_t>& signature,
                    const std::vector<uint8_t>& data,
                    const std::vector<uint8_t>& public_key));
  MOCK_METHOD3(Sign,
               bool(const std::vector<uint8_t>& data,
                    crypto::RSAPrivateKey* key,
                    std::vector<uint8_t>* out_signature));
  base::FilePath GetOwnerKeyFilePath() override;

  PK11SlotInfo* GetSlot();

  // After this is called, OpenUserDB() will return empty ScopedPK11Slots.
  void MakeBadDB() { return_bad_db_ = true; }

  // Ensures that temp_dir_ is created and accessible.
  bool EnsureTempDir();

 protected:
  bool return_bad_db_ = false;
  crypto::ScopedTestNSSDB test_nssdb_;
  base::ScopedTempDir temp_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNssUtil);
};

class CheckPublicKeyUtil : public MockNssUtil {
 public:
  explicit CheckPublicKeyUtil(bool expected);
  ~CheckPublicKeyUtil() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CheckPublicKeyUtil);
};

class KeyCheckUtil : public MockNssUtil {
 public:
  KeyCheckUtil();
  ~KeyCheckUtil() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyCheckUtil);
};

class KeyFailUtil : public MockNssUtil {
 public:
  KeyFailUtil();
  ~KeyFailUtil() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyFailUtil);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_NSS_UTIL_H_
