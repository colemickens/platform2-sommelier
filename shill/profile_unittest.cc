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
#include "shill/mock_store.h"
#include "shill/property_store_unittest.h"

using std::set;
using std::string;
using std::vector;
using testing::_;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

namespace shill {

class ProfileTest : public PropertyStoreTest {
 public:
  ProfileTest()
      : profile_(new MockProfile(control_interface(), manager(), "")) {
  }

  MockService *CreateMockService() {
    return new StrictMock<MockService>(control_interface(),
                                       dispatcher(),
                                       manager());
  }

 protected:
  MockStore store_;
  scoped_refptr<MockProfile> profile_;
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

TEST_F(ProfileTest, GetFriendlyName) {
  static const char kUser[] = "theUser";
  static const char kIdentifier[] = "theIdentifier";
  Profile::Identifier id;
  id.identifier = kIdentifier;
  ProfileRefPtr profile(
      new Profile(control_interface(), manager(), id, "", false));
  EXPECT_EQ(kIdentifier, profile->GetFriendlyName());
  id.user = kUser;
  profile = new Profile(control_interface(), manager(), id, "", false);
  EXPECT_EQ(string(kUser) + "/" + kIdentifier, profile->GetFriendlyName());
}

TEST_F(ProfileTest, GetStoragePath) {
  static const char kUser[] = "chronos";
  static const char kIdentifier[] = "someprofile";
  static const char kFormat[] = "/a/place/for/%s";
  FilePath path;
  Profile::Identifier id;
  id.identifier = kIdentifier;
  ProfileRefPtr profile(
      new Profile(control_interface(), manager(), id, "", false));
  EXPECT_FALSE(profile->GetStoragePath(&path));
  id.user = kUser;
  profile =
      new Profile(control_interface(), manager(), id, kFormat, false);
  EXPECT_TRUE(profile->GetStoragePath(&path));
  string suffix = base::StringPrintf("/%s.profile", kIdentifier);
  EXPECT_EQ(base::StringPrintf(kFormat, kUser) + suffix, path.value());
}

TEST_F(ProfileTest, ServiceManagement) {
  string service1_name;
  string service2_name;
  {
    ServiceRefPtr service1(CreateMockService());
    ServiceRefPtr service2(CreateMockService());
    service1_name = service1->UniqueName();
    service2_name = service2->UniqueName();
    ASSERT_TRUE(profile_->AdoptService(service1));
    ASSERT_TRUE(profile_->AdoptService(service2));
    ASSERT_FALSE(profile_->AdoptService(service1));
  }
  ASSERT_TRUE(profile_->FindService(service1_name).get() != NULL);
  ASSERT_TRUE(profile_->FindService(service2_name).get() != NULL);

  ASSERT_TRUE(profile_->AbandonService(service1_name));
  ASSERT_TRUE(profile_->FindService(service1_name).get() == NULL);
  ASSERT_TRUE(profile_->FindService(service2_name).get() != NULL);

  ASSERT_FALSE(profile_->AbandonService(service1_name));
  ASSERT_TRUE(profile_->AbandonService(service2_name));
  ASSERT_TRUE(profile_->FindService(service1_name).get() == NULL);
  ASSERT_TRUE(profile_->FindService(service2_name).get() == NULL);
}

TEST_F(ProfileTest, Finalize) {
  scoped_refptr<MockService> service1(CreateMockService());
  scoped_refptr<MockService> service2(CreateMockService());
  EXPECT_CALL(*service1.get(), Save(_)).WillOnce(Return(true));
  EXPECT_CALL(*service2.get(), Save(_)).WillOnce(Return(true));

  ASSERT_TRUE(profile_->AdoptService(service1));
  ASSERT_TRUE(profile_->AdoptService(service2));

  profile_->Finalize(&store_);
}

TEST_F(ProfileTest, EntryEnumeration) {
  scoped_refptr<MockService> service1(
      new StrictMock<MockService>(control_interface(),
                                  dispatcher(),
                                  manager()));
  scoped_refptr<MockService> service2(
      new StrictMock<MockService>(control_interface(),
                                  dispatcher(),
                                  manager()));
  string service1_name(service1->UniqueName());
  string service2_name(service2->UniqueName());

  EXPECT_CALL(*service1.get(), GetRpcIdentifier())
      .WillRepeatedly(Return(service1_name));
  EXPECT_CALL(*service2.get(), GetRpcIdentifier())
      .WillRepeatedly(Return(service2_name));
  ASSERT_TRUE(profile_->AdoptService(service1));
  ASSERT_TRUE(profile_->AdoptService(service2));

  ASSERT_EQ(profile_->EnumerateEntries().size(), 2);

  ASSERT_TRUE(profile_->AbandonService(service1_name));
  ASSERT_EQ(profile_->EnumerateEntries()[0], service2_name);

  ASSERT_FALSE(profile_->AbandonService(service1_name));
  ASSERT_EQ(profile_->EnumerateEntries()[0], service2_name);

  ASSERT_TRUE(profile_->AbandonService(service2_name));
  ASSERT_EQ(profile_->EnumerateEntries().size(), 0);
}

}  // namespace shill
