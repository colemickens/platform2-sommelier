// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/mock_nss_util.h"

#include <unistd.h>

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <crypto/rsa_private_key.h>

namespace login_manager {
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::_;

MockNssUtil::MockNssUtil() {}
MockNssUtil::~MockNssUtil() {}

// static
crypto::RSAPrivateKey* MockNssUtil::CreateShortKey() {
  crypto::RSAPrivateKey* ret = crypto::RSAPrivateKey::CreateSensitive(256);
  LOG_IF(ERROR, ret == NULL) << "returning NULL!!!";
  return ret;
}

CheckPublicKeyUtil::CheckPublicKeyUtil(bool expected) {
  EXPECT_CALL(*this, CheckPublicKeyBlob(_)).WillOnce(Return(expected));
}

CheckPublicKeyUtil::~CheckPublicKeyUtil() {}

KeyCheckUtil::KeyCheckUtil() {
  EXPECT_CALL(*this, OpenUserDB()).WillOnce(Return(true));
  ON_CALL(*this, GetPrivateKey(_))
      .WillByDefault(InvokeWithoutArgs(CreateShortKey));
  EXPECT_CALL(*this, GetPrivateKey(_)).Times(1);
}

KeyCheckUtil::~KeyCheckUtil() {}

KeyFailUtil::KeyFailUtil() {
  EXPECT_CALL(*this, OpenUserDB()).WillOnce(Return(true));
  EXPECT_CALL(*this, GetPrivateKey(_))
      .WillOnce(Return(reinterpret_cast<crypto::RSAPrivateKey*>(NULL)));
}

KeyFailUtil::~KeyFailUtil() {}

SadNssUtil::SadNssUtil() {
  EXPECT_CALL(*this, OpenUserDB()).WillOnce(Return(false));
}

SadNssUtil::~SadNssUtil() {}

}  // namespace login_manager
