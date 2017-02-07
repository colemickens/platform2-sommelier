//
// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "trunks/scoped_global_session.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "trunks/mock_hmac_session.h"
#include "trunks/mock_session_manager.h"
#include "trunks/trunks_factory_for_test.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

namespace trunks {

class ScopedGlobalHmacSessionTest : public testing::Test {
 public:
  void SetUp() override {
    factory_.set_session_manager(&session_manager_);
    factory_.set_hmac_session(&hmac_session_);
  }

 protected:
  StrictMock<MockHmacSession> hmac_session_;
  NiceMock<MockSessionManager> session_manager_;
  TrunksFactoryForTest factory_;
};

#ifdef TRUNKS_USE_PER_OP_SESSIONS
TEST_F(ScopedGlobalHmacSessionTest, HmacSessionSuccessNew) {
  for (bool enable_encryption : {true, false}) {
    std::unique_ptr<HmacSession> global_session;
    EXPECT_EQ(nullptr, global_session);
    {
      EXPECT_CALL(hmac_session_, StartUnboundSession(enable_encryption))
          .WillOnce(Return(TPM_RC_SUCCESS));
      ScopedGlobalHmacSession scope(&factory_, enable_encryption,
                                    &global_session);
      EXPECT_NE(nullptr, global_session);
    }
    EXPECT_EQ(nullptr, global_session);
  }
}

TEST_F(ScopedGlobalHmacSessionTest, HmacSessionFailureNew) {
  for (bool enable_encryption : {true, false}) {
    std::unique_ptr<HmacSession> global_session;
    {
      EXPECT_CALL(hmac_session_, StartUnboundSession(enable_encryption))
          .WillOnce(Return(TPM_RC_FAILURE));
      ScopedGlobalHmacSession scope(&factory_, enable_encryption,
                                    &global_session);
      EXPECT_EQ(nullptr, global_session);
    }
    EXPECT_EQ(nullptr, global_session);
  }
}

TEST_F(ScopedGlobalHmacSessionTest, HmacSessionSuccessExisting) {
  for (bool enable_encryption : {true, false}) {
    auto old_hmac_session = new StrictMock<MockHmacSession>();
    std::unique_ptr<HmacSession> global_session(old_hmac_session);
    EXPECT_EQ(old_hmac_session, global_session.get());
    {
      EXPECT_CALL(hmac_session_, StartUnboundSession(enable_encryption))
          .WillOnce(Return(TPM_RC_SUCCESS));
      ScopedGlobalHmacSession scope(&factory_, enable_encryption,
                                    &global_session);
      EXPECT_NE(nullptr, global_session);
      EXPECT_NE(old_hmac_session, global_session.get());
    }
    EXPECT_EQ(nullptr, global_session);
  }
}

TEST_F(ScopedGlobalHmacSessionTest, HmacSessionFailureExisting) {
  for (bool enable_encryption : {true, false}) {
    auto old_hmac_session = new StrictMock<MockHmacSession>();
    std::unique_ptr<HmacSession> global_session(old_hmac_session);
    EXPECT_EQ(old_hmac_session, global_session.get());
    {
      EXPECT_CALL(hmac_session_, StartUnboundSession(enable_encryption))
          .WillOnce(Return(TPM_RC_FAILURE));
      ScopedGlobalHmacSession scope(&factory_, enable_encryption,
                                    &global_session);
      EXPECT_EQ(nullptr, global_session);
    }
    EXPECT_EQ(nullptr, global_session);
  }
}
#else  // TRUNKS_USE_PER_OP_SESSIONS
TEST_F(ScopedGlobalHmacSessionTest, HmacSessionNew) {
  for (bool enable_encryption : {true, false}) {
    std::unique_ptr<HmacSession> global_session;
    {
      ScopedGlobalHmacSession scope(&factory_, enable_encryption,
                                    &global_session);
      EXPECT_EQ(nullptr, global_session);
    }
    EXPECT_EQ(nullptr, global_session);
  }
}

TEST_F(ScopedGlobalHmacSessionTest, HmacSessionExisting) {
  for (bool enable_encryption : {true, false}) {
    auto old_hmac_session = new StrictMock<MockHmacSession>();
    std::unique_ptr<HmacSession> global_session(old_hmac_session);
    {
      ScopedGlobalHmacSession scope(&factory_, enable_encryption,
                                    &global_session);
      EXPECT_EQ(old_hmac_session, global_session.get());
    }
    EXPECT_EQ(old_hmac_session, global_session.get());
  }
}
#endif  // TRUNKS_USE_PER_OP_SESSIONS

}  // namespace trunks
