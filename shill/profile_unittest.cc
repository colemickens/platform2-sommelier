// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/profile.h"

#include <string>
#include <vector>

#include <base/string_util.h>
#include <gtest/gtest.h>

#include "shill/mock_profile.h"
#include "shill/mock_service.h"
#include "shill/property_store_unittest.h"

using std::set;
using std::string;
using std::vector;
using testing::Return;
using testing::StrictMock;

namespace shill {

class ProfileTest : public PropertyStoreTest {
 public:
  ProfileTest()
      : profile_(new Profile(&control_interface_, &glib_, &manager_)) {
  }

 protected:
  ProfileRefPtr profile_;
};

TEST_F(ProfileTest, IsValidIdentifierToken) {
  EXPECT_FALSE(Profile::IsValidIdentifierToken(""));
  EXPECT_FALSE(Profile::IsValidIdentifierToken(" "));
  EXPECT_FALSE(Profile::IsValidIdentifierToken("-"));
  EXPECT_FALSE(Profile::IsValidIdentifierToken("~"));
  EXPECT_FALSE(Profile::IsValidIdentifierToken("_"));
  EXPECT_TRUE(Profile::IsValidIdentifierToken("a"));
  EXPECT_TRUE(Profile::IsValidIdentifierToken("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
  EXPECT_TRUE(Profile::IsValidIdentifierToken("abcdefghijklmnopqrstuvwxyz"));
  EXPECT_TRUE(Profile::IsValidIdentifierToken("0123456789"));
}

TEST_F(ProfileTest, ParseIdentifier) {
  Profile::Identifier identifier;
  EXPECT_FALSE(Profile::ParseIdentifier("", &identifier));
  EXPECT_FALSE(Profile::ParseIdentifier("~", &identifier));
  EXPECT_FALSE(Profile::ParseIdentifier("~foo", &identifier));
  EXPECT_FALSE(Profile::ParseIdentifier("~/", &identifier));
  EXPECT_FALSE(Profile::ParseIdentifier("~bar/", &identifier));
  EXPECT_FALSE(Profile::ParseIdentifier("~/zoo", &identifier));
  EXPECT_FALSE(Profile::ParseIdentifier("~./moo", &identifier));
  EXPECT_FALSE(Profile::ParseIdentifier("~valid/?", &identifier));
  EXPECT_FALSE(Profile::ParseIdentifier("~no//no", &identifier));
  EXPECT_FALSE(Profile::ParseIdentifier("~no~no", &identifier));

  static const char kUser[] = "user";
  static const char kIdentifier[] = "identifier";
  EXPECT_TRUE(Profile::ParseIdentifier(
      base::StringPrintf("~%s/%s", kUser, kIdentifier),
      &identifier));
  EXPECT_EQ(kUser, identifier.user);
  EXPECT_EQ(kIdentifier, identifier.identifier);

  EXPECT_FALSE(Profile::ParseIdentifier("!", &identifier));
  EXPECT_FALSE(Profile::ParseIdentifier("/nope", &identifier));

  static const char kIdentifier2[] = "something";
  EXPECT_TRUE(Profile::ParseIdentifier(kIdentifier2, &identifier));
  EXPECT_EQ("", identifier.user);
  EXPECT_EQ(kIdentifier2, identifier.identifier);
}

TEST_F(ProfileTest, GetRpcPath) {
  static const char kUser[] = "theUser";
  static const char kIdentifier[] = "theIdentifier";
  static const char kPathPrefix[] = "/profile/";
  Profile::Identifier identifier;
  identifier.identifier = kIdentifier;
  EXPECT_EQ(string(kPathPrefix) + kIdentifier, Profile::GetRpcPath(identifier));
  identifier.user = kUser;
  EXPECT_EQ(string(kPathPrefix) + kUser + "/" + kIdentifier,
            Profile::GetRpcPath(identifier));
}

TEST_F(ProfileTest, GetStoragePath) {
  static const char kUser[] = "chronos";
  static const char kIdentifier[] = "someprofile";
  FilePath path;
  Profile::Identifier identifier;
  identifier.identifier = kIdentifier;
  EXPECT_TRUE(Profile::GetStoragePath(identifier, &path));
  EXPECT_EQ("/var/cache/flimflam/someprofile.profile", path.value());
  identifier.user = kUser;
  EXPECT_TRUE(Profile::GetStoragePath(identifier, &path));
  EXPECT_EQ("/home/chronos/user/flimflam/someprofile.profile", path.value());
}

TEST_F(ProfileTest, ServiceManagement) {
  const char kService1[] = "service1";
  const char kService2[] = "wifi_service2";
  {
    ServiceRefPtr service1(
        new StrictMock<MockService>(&control_interface_,
                                    &dispatcher_,
                                    &manager_,
                                    kService1));
    ServiceRefPtr service2(
        new StrictMock<MockService>(&control_interface_,
                                    &dispatcher_,
                                    &manager_,
                                    kService2));
    ASSERT_TRUE(profile_->AdoptService(service1));
    ASSERT_TRUE(profile_->AdoptService(service2));
    ASSERT_FALSE(profile_->AdoptService(service1));
  }
  ASSERT_TRUE(profile_->FindService(kService1).get() != NULL);
  ASSERT_TRUE(profile_->FindService(kService2).get() != NULL);

  ASSERT_TRUE(profile_->AbandonService(kService1));
  ASSERT_TRUE(profile_->FindService(kService1).get() == NULL);
  ASSERT_TRUE(profile_->FindService(kService2).get() != NULL);

  ASSERT_FALSE(profile_->AbandonService(kService1));
  ASSERT_TRUE(profile_->AbandonService(kService2));
  ASSERT_TRUE(profile_->FindService(kService1).get() == NULL);
  ASSERT_TRUE(profile_->FindService(kService2).get() == NULL);
}

TEST_F(ProfileTest, EntryEnumeration) {
  const char *kServices[] = { "service1",
                              "wifi_service2" };
  scoped_refptr<MockService> service1(
      new StrictMock<MockService>(&control_interface_,
                                  &dispatcher_,
                                  &manager_,
                                  kServices[0]));
  EXPECT_CALL(*service1.get(), GetRpcIdentifier())
      .WillRepeatedly(Return(kServices[0]));
  scoped_refptr<MockService> service2(
      new StrictMock<MockService>(&control_interface_,
                                  &dispatcher_,
                                  &manager_,
                                  kServices[1]));
  EXPECT_CALL(*service2.get(), GetRpcIdentifier())
      .WillRepeatedly(Return(kServices[1]));
  ASSERT_TRUE(profile_->AdoptService(service1));
  ASSERT_TRUE(profile_->AdoptService(service2));

  ASSERT_EQ(profile_->EnumerateEntries().size(), 2);

  ASSERT_TRUE(profile_->AbandonService(kServices[0]));
  ASSERT_EQ(profile_->EnumerateEntries()[0], kServices[1]);

  ASSERT_FALSE(profile_->AbandonService(kServices[0]));
  ASSERT_EQ(profile_->EnumerateEntries()[0], kServices[1]);

  ASSERT_TRUE(profile_->AbandonService(kServices[1]));
  ASSERT_EQ(profile_->EnumerateEntries().size(), 0);
}

} // namespace shill
