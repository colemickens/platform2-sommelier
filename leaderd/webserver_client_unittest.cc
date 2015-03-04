// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leaderd/webserver_client.h"

#include <gtest/gtest.h>

namespace leaderd {

namespace {
const char kLeadershipScoreKey[] = "score";
const char kLeadershipGroupKey[] = "group";
const char kLeadershipIdKey[] = "uuid";
const char kLeadershipLeaderKey[] = "leader";
const char kGroupGUID[] = "ABC";
const char kChallengeUUID[] = "DEF";
const char kMyUUID[] = "GHI";
}  // namespace

class WebServerClientTest : public testing::Test,
                            public WebServerClient::Delegate {
 public:
  WebServerClientTest() : webserver_(this) {}

  std::unique_ptr<base::DictionaryValue> ChallengeRequestHandler(
      const base::DictionaryValue* input) {
    return webserver_.ProcessChallenge(input);
  }

  void SetWebServerPort(uint16_t port) override {}

  bool ChallengeLeader(const std::string& in_uuid, const std::string& in_guid,
                       int32_t in_score, std::string* out_leader,
                       std::string* out_my_uuid) override {
    if (in_guid != kGroupGUID) return false;
    *out_leader = in_uuid;
    *out_my_uuid = kMyUUID;
    return true;
  }

 private:
  WebServerClient webserver_;
};

TEST_F(WebServerClientTest, BadData) {
  EXPECT_EQ(nullptr, ChallengeRequestHandler(nullptr));
}

TEST_F(WebServerClientTest, UnknownFields) {
  base::DictionaryValue input;
  input.SetString("BogusField", kGroupGUID);
  EXPECT_EQ(nullptr, ChallengeRequestHandler(&input));
}

TEST_F(WebServerClientTest, GroupMissing) {
  base::DictionaryValue input;
  input.SetInteger(kLeadershipScoreKey, 23);
  input.SetString(kLeadershipIdKey, kChallengeUUID);
  EXPECT_EQ(nullptr, ChallengeRequestHandler(&input));
}

TEST_F(WebServerClientTest, UUIDMissing) {
  base::DictionaryValue input;
  input.SetInteger(kLeadershipScoreKey, 23);
  input.SetString(kLeadershipGroupKey, kGroupGUID);
  EXPECT_EQ(nullptr, ChallengeRequestHandler(&input));
}

TEST_F(WebServerClientTest, ScoreMissing) {
  base::DictionaryValue input;
  input.SetString(kLeadershipGroupKey, kGroupGUID);
  input.SetString(kLeadershipIdKey, kChallengeUUID);
  EXPECT_EQ(nullptr, ChallengeRequestHandler(&input));
}

TEST_F(WebServerClientTest, ScoreAsTextFail) {
  base::DictionaryValue input;
  input.SetString(kLeadershipScoreKey, "23");
  input.SetString(kLeadershipGroupKey, kGroupGUID);
  input.SetString(kLeadershipIdKey, kChallengeUUID);
  EXPECT_EQ(nullptr, ChallengeRequestHandler(&input));
}

TEST_F(WebServerClientTest, UnknownGroup) {
  base::DictionaryValue input;
  input.SetInteger(kLeadershipScoreKey, 23);
  input.SetString(kLeadershipGroupKey, "1235");
  input.SetString(kLeadershipIdKey, kChallengeUUID);
  EXPECT_EQ(nullptr, ChallengeRequestHandler(&input));
}

TEST_F(WebServerClientTest, Success) {
  base::DictionaryValue input;
  input.SetInteger(kLeadershipScoreKey, 23);
  input.SetString(kLeadershipGroupKey, kGroupGUID);
  input.SetString(kLeadershipIdKey, kChallengeUUID);
  std::unique_ptr<base::DictionaryValue> output =
      ChallengeRequestHandler(&input);
  EXPECT_NE(nullptr, output);
  std::string leader;
  EXPECT_TRUE(output->GetString(kLeadershipLeaderKey, &leader));
  EXPECT_EQ(kChallengeUUID, leader);
  std::string my_uuid;
  EXPECT_TRUE(output->GetString(kLeadershipIdKey, &my_uuid));
  EXPECT_EQ(kMyUUID, my_uuid);
}

}  // namespace leaderd
