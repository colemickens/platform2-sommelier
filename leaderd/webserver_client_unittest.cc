// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leaderd/webserver_client.h"

#include <gtest/gtest.h>

namespace leaderd {

namespace {

const char kChallengeGroupKey[] = "group";
const char kChallengeIdKey[] = "uuid";
const char kChallengeScoreKey[] = "score";

const char kAnnounceGroupKey[] = "group";
const char kAnnounceLeaderIdKey[] = "my_uuid";
const char kAnnounceScoreKey[] = "score";

const char kGroupId[] = "ABC";
}  // namespace

class WebServerClientTest : public testing::Test,
                            public WebServerClient::Delegate {
 public:
  WebServerClientTest() : webserver_(this, "protocol_handler_name") {}

  std::unique_ptr<base::DictionaryValue> ProcessChallenge(
      const base::DictionaryValue* input) {
    return webserver_.ProcessChallenge(input);
  }

  bool ProcessAnnouncement(const base::DictionaryValue* input) {
    return webserver_.ProcessAnnouncement(input);
  }

  void SetWebServerPort(uint16_t port) override {}

  bool HandleLeaderChallenge(const std::string& in_guid,
                             const std::string& in_uuid,
                             int32_t in_score,
                             std::string* out_leader,
                             std::string* out_my_uuid) override {
    if (in_guid != kGroupId) return false;
    *out_leader = in_uuid;
    *out_my_uuid = "This is my own ID.";
    return true;
  }

  bool HandleLeaderAnnouncement(const std::string& group_id,
                                const std::string& leader_id,
                                int32_t leader_score) override {
    if (group_id != kGroupId) return false;
    return true;
  }

  std::unique_ptr<base::DictionaryValue> GetValidChallengeInput() {
    std::unique_ptr<base::DictionaryValue> input{new base::DictionaryValue()};
    input->SetInteger(kChallengeScoreKey, 23);
    input->SetString(kChallengeGroupKey, kGroupId);
    input->SetString(kChallengeIdKey, "this is the challenger's ID");
    return input;
  }

  std::unique_ptr<base::DictionaryValue> GetValidAnnouncementInput() {
    std::unique_ptr<base::DictionaryValue> input{new base::DictionaryValue()};
    input->SetInteger(kAnnounceScoreKey, 23);
    input->SetString(kAnnounceGroupKey, kGroupId);
    input->SetString(kAnnounceLeaderIdKey, "This is the leader's ID");
    return input;
  }

 private:
  WebServerClient webserver_;
};

TEST_F(WebServerClientTest, ChallengeBadData) {
  EXPECT_EQ(nullptr, ProcessChallenge(nullptr));
}

TEST_F(WebServerClientTest, ChallengeRejectsExtraFields) {
  auto input = GetValidChallengeInput();
  input->SetString("BogusField", kGroupId);
  EXPECT_EQ(nullptr, ProcessChallenge(input.get()));
}

TEST_F(WebServerClientTest, ChallengeRejectsMissingFields) {
  // We need group to exist.
  auto input = GetValidChallengeInput();
  input->Remove(kChallengeGroupKey, nullptr);
  EXPECT_EQ(nullptr, ProcessChallenge(input.get()));
  // Similarly, the challenger id
  input = GetValidChallengeInput();
  input->Remove(kChallengeIdKey, nullptr);
  EXPECT_EQ(nullptr, ProcessChallenge(input.get()));
  // Similarly, the score.
  input = GetValidChallengeInput();
  input->Remove(kChallengeScoreKey, nullptr);
  EXPECT_EQ(nullptr, ProcessChallenge(input.get()));
}

TEST_F(WebServerClientTest, ChallengeScoreAsTextFail) {
  auto input = GetValidChallengeInput();
  input->SetString(kChallengeScoreKey, "23");
  EXPECT_EQ(nullptr, ProcessChallenge(input.get()));
}

TEST_F(WebServerClientTest, ChallengeDelegateFails) {
  auto input = GetValidChallengeInput();
  input->SetString(kChallengeGroupKey, "not-the-expected-value");
  EXPECT_EQ(nullptr, ProcessChallenge(input.get()));
}

TEST_F(WebServerClientTest, ChallengeDelegateSuccess) {
  auto input = GetValidChallengeInput();
  std::unique_ptr<base::DictionaryValue> output = ProcessChallenge(input.get());
  EXPECT_NE(nullptr, output);
}

TEST_F(WebServerClientTest, AnnouncementBadData) {
  EXPECT_FALSE(ProcessAnnouncement(nullptr));
}

TEST_F(WebServerClientTest, AnnouncementRejectsExtraFields) {
  auto input = GetValidAnnouncementInput();
  input->SetString("BogusField", kGroupId);
  EXPECT_FALSE(ProcessAnnouncement(input.get()));
}

TEST_F(WebServerClientTest, AnnouncementRejectsMissingFields) {
  // We need group to exist.
  auto input = GetValidAnnouncementInput();
  input->Remove(kAnnounceGroupKey, nullptr);
  EXPECT_FALSE(ProcessAnnouncement(input.get()));
  // Similarly, the leader id
  input = GetValidAnnouncementInput();
  input->Remove(kAnnounceLeaderIdKey, nullptr);
  EXPECT_FALSE(ProcessAnnouncement(input.get()));
  // Similarly, the score.
  input = GetValidAnnouncementInput();
  input->Remove(kAnnounceScoreKey, nullptr);
  EXPECT_FALSE(ProcessAnnouncement(input.get()));
}

TEST_F(WebServerClientTest, AnnouncementScoreAsTextFail) {
  auto input = GetValidAnnouncementInput();
  input->SetString(kAnnounceScoreKey, "23");
  EXPECT_FALSE(ProcessAnnouncement(input.get()));
}

TEST_F(WebServerClientTest, AnnouncementDelegateFails) {
  auto input = GetValidAnnouncementInput();
  input->SetString(kAnnounceGroupKey, "not-the-expected-value");
  EXPECT_FALSE(ProcessAnnouncement(input.get()));
}

TEST_F(WebServerClientTest, AnnouncementDelegateSuccess) {
  auto input = GetValidAnnouncementInput();
  EXPECT_TRUE(ProcessAnnouncement(input.get()));
}

}  // namespace leaderd
