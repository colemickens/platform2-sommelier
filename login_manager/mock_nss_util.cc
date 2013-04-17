// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/mock_nss_util.h"

#include <secmodt.h>
#include <unistd.h>

#include <base/file_path.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <crypto/nss_util.h>
#include <crypto/nss_util_internal.h>
#include <crypto/rsa_private_key.h>
#include <crypto/scoped_nss_types.h>

namespace login_manager {
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::_;

using crypto::GetPrivateNSSKeySlot;
using crypto::ScopedPK11Slot;

MockNssUtil::MockNssUtil()
    : return_bad_db_(false),
      slot_(GetPrivateNSSKeySlot()) {
  ON_CALL(*this, GetNssdbSubpath()).WillByDefault(Return(base::FilePath()));
}
MockNssUtil::~MockNssUtil() {}

crypto::ScopedPK11Slot MockNssUtil::OpenUserDB(
    const base::FilePath& user_homedir) {
  if (return_bad_db_)
    return crypto::ScopedPK11Slot();
  return crypto::ScopedPK11Slot(GetPrivateNSSKeySlot());
}

// static
crypto::RSAPrivateKey* MockNssUtil::CreateShortKey() {
  crypto::RSAPrivateKey* ret = crypto::RSAPrivateKey::CreateSensitive(256);
  LOG_IF(ERROR, ret == NULL) << "returning NULL!!!";
  return ret;
}

PK11SlotInfo* MockNssUtil::GetSlot() {
  return slot_.get();
}

CheckPublicKeyUtil::CheckPublicKeyUtil(bool expected) {
  EXPECT_CALL(*this, CheckPublicKeyBlob(_)).WillOnce(Return(expected));
}

CheckPublicKeyUtil::~CheckPublicKeyUtil() {}

KeyCheckUtil::KeyCheckUtil() {
  ON_CALL(*this, GetPrivateKeyForUser(_, _))
      .WillByDefault(InvokeWithoutArgs(CreateShortKey));
  EXPECT_CALL(*this, GetPrivateKeyForUser(_, _)).Times(1);
}

KeyCheckUtil::~KeyCheckUtil() {}

KeyFailUtil::KeyFailUtil() {
  EXPECT_CALL(*this, GetPrivateKeyForUser(_, _))
      .WillOnce(Return(reinterpret_cast<crypto::RSAPrivateKey*>(NULL)));
}

KeyFailUtil::~KeyFailUtil() {}

}  // namespace login_manager
