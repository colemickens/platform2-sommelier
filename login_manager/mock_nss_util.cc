// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/mock_nss_util.h"

#include <unistd.h>
#include <base/crypto/rsa_private_key.h>
#include <base/logging.h>

namespace login_manager {
using ::testing::Invoke;
using ::testing::Return;
using ::testing::_;

MockNssUtil::MockNssUtil() {}
MockNssUtil::~MockNssUtil() {}

void MockNssUtil::ExpectGetOwnerKeyFilePath() {
  EXPECT_CALL(*this, GetOwnerKeyFilePath()).WillOnce(Return(FilePath("")));
}

KeyCheckUtil::KeyCheckUtil() {
  ExpectGetOwnerKeyFilePath();
  EXPECT_CALL(*this, MightHaveKeys()).WillOnce(Return(true));
  EXPECT_CALL(*this, OpenUserDB()).WillOnce(Return(true));
  EXPECT_CALL(*this, GetPrivateKey(_))
      .WillOnce(Return(reinterpret_cast<base::RSAPrivateKey*>(7)));
}
KeyCheckUtil::~KeyCheckUtil() {}

KeyFailUtil::KeyFailUtil() {
  ExpectGetOwnerKeyFilePath();
  EXPECT_CALL(*this, MightHaveKeys()).WillOnce(Return(true));
  EXPECT_CALL(*this, OpenUserDB()).WillOnce(Return(true));
  EXPECT_CALL(*this, GetPrivateKey(_))
      .WillOnce(Return(reinterpret_cast<base::RSAPrivateKey*>(NULL)));
}
KeyFailUtil::~KeyFailUtil() {}

SadNssUtil::SadNssUtil() {
  ExpectGetOwnerKeyFilePath();
  EXPECT_CALL(*this, MightHaveKeys()).WillOnce(Return(true));
  EXPECT_CALL(*this, OpenUserDB()).WillOnce(Return(false));
}
SadNssUtil::~SadNssUtil() {}

EmptyNssUtil::EmptyNssUtil() {
  ExpectGetOwnerKeyFilePath();
  EXPECT_CALL(*this, MightHaveKeys()).WillOnce(Return(false));
}
EmptyNssUtil::~EmptyNssUtil() {}

ShortKeyGenerator::ShortKeyGenerator() {
  base::EnsureNSSInit();
  base::OpenPersistentNSSDB();
  ON_CALL(*this, GenerateKeyPair()).WillByDefault(Invoke(CreateFake));
}
ShortKeyGenerator::~ShortKeyGenerator() {}

// static
base::RSAPrivateKey* ShortKeyGenerator::CreateFake() {
  base::RSAPrivateKey* ret = base::RSAPrivateKey::CreateSensitive(512);
  LOG_IF(INFO, ret == NULL) << "returning NULL!!!";
  return ret;
}

ShortKeyUtil::ShortKeyUtil() {
  ExpectGetOwnerKeyFilePath();
  EXPECT_CALL(*this, MightHaveKeys()).WillOnce(Return(true));
  EXPECT_CALL(*this, OpenUserDB()).WillOnce(Return(true));
  EXPECT_CALL(*this, GenerateKeyPair()).Times(1);
}
ShortKeyUtil::~ShortKeyUtil() {}

}  // namespace login_manager
