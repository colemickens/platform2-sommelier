// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/profile.h"

#include <string>
#include <vector>

#include <base/file_path.h>
#include <base/memory/scoped_ptr.h>
#include <base/string_util.h>
#include <gtest/gtest.h>

#include "shill/glib.h"
#include "shill/key_file_store.h"
#include "shill/mock_profile.h"
#include "shill/mock_service.h"
#include "shill/mock_store.h"
#include "shill/property_store_unittest.h"
#include "shill/service_under_test.h"

using std::set;
using std::string;
using std::vector;
using testing::_;
using testing::Invoke;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

namespace shill {

class ProfileTest : public PropertyStoreTest {
 public:
  ProfileTest() {
    Profile::Identifier id("rather", "irrelevant");
    profile_ = new Profile(control_interface(), manager(), id, "", false);
  }

  MockService *CreateMockService() {
    return new StrictMock<MockService>(control_interface(),
                                       dispatcher(),
                                       metrics(),
                                       manager());
  }

  virtual void SetUp() {
    PropertyStoreTest::SetUp();
    FilePath final_path(storage_path());
    final_path = final_path.Append("test.profile");
    scoped_ptr<KeyFileStore> storage(new KeyFileStore(&real_glib_));
    storage->set_path(final_path);
    ASSERT_TRUE(storage->Open());
    profile_->set_storage(storage.release());  // Passes ownership.
  }

  bool ProfileInitStorage(const Profile::Identifier &id,
                          Profile::InitStorageOption storage_option,
                          bool save,
                          Error::Type error_type) {
    Error error;
    ProfileRefPtr profile(
        new Profile(control_interface(), manager(), id, storage_path(), false));
    bool ret = profile->InitStorage(&real_glib_, storage_option, &error);
    EXPECT_EQ(error_type, error.type());
    if (ret && save) {
      EXPECT_TRUE(profile->Save());
    }
    return ret;
  }

 protected:
  GLib real_glib_;
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

TEST_F(ProfileTest, GetFriendlyName) {
  static const char kUser[] = "theUser";
  static const char kIdentifier[] = "theIdentifier";
  Profile::Identifier id(kIdentifier);
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
  Profile::Identifier id(kIdentifier);
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
  scoped_refptr<MockService> service1(CreateMockService());
  scoped_refptr<MockService> service2(CreateMockService());

  EXPECT_CALL(*service1.get(), Save(_))
      .WillRepeatedly(Invoke(service1.get(), &MockService::FauxSave));
  EXPECT_CALL(*service2.get(), Save(_))
      .WillRepeatedly(Invoke(service2.get(), &MockService::FauxSave));

  ASSERT_TRUE(profile_->AdoptService(service1));
  ASSERT_TRUE(profile_->AdoptService(service2));

  // Ensure services are in the profile now.
  ASSERT_TRUE(profile_->ContainsService(service1));
  ASSERT_TRUE(profile_->ContainsService(service2));

  // Ensure we can't add them twice.
  ASSERT_FALSE(profile_->AdoptService(service1));
  ASSERT_FALSE(profile_->AdoptService(service2));

  // Ensure that we can abandon individually, and that doing so is idempotent.
  ASSERT_TRUE(profile_->AbandonService(service1));
  ASSERT_FALSE(profile_->ContainsService(service1));
  ASSERT_TRUE(profile_->AbandonService(service1));
  ASSERT_TRUE(profile_->ContainsService(service2));

  // Clean up.
  ASSERT_TRUE(profile_->AbandonService(service2));
  ASSERT_FALSE(profile_->ContainsService(service1));
  ASSERT_FALSE(profile_->ContainsService(service2));
}

TEST_F(ProfileTest, ServiceConfigure) {
  ServiceRefPtr service1(new ServiceUnderTest(control_interface(),
                                              dispatcher(),
                                              metrics(),
                                              manager()));
  service1->set_priority(service1->priority() + 1);  // Change from default.
  ASSERT_TRUE(profile_->AdoptService(service1));
  ASSERT_TRUE(profile_->ContainsService(service1));

  // Create new service; ask Profile to merge it with a known, matching,
  // service; ensure that settings from |service1| wind up in |service2|.
  ServiceRefPtr service2(new ServiceUnderTest(control_interface(),
                                              dispatcher(),
                                              metrics(),
                                              manager()));
  int32 orig_priority = service2->priority();
  ASSERT_TRUE(profile_->ConfigureService(service2));
  ASSERT_EQ(service1->priority(), service2->priority());
  ASSERT_NE(orig_priority, service2->priority());

  // Clean up.
  ASSERT_TRUE(profile_->AbandonService(service1));
  ASSERT_FALSE(profile_->ContainsService(service1));
  ASSERT_FALSE(profile_->ContainsService(service2));
}

TEST_F(ProfileTest, Save) {
  scoped_refptr<MockService> service1(CreateMockService());
  scoped_refptr<MockService> service2(CreateMockService());
  EXPECT_CALL(*service1.get(), Save(_)).WillOnce(Return(true));
  EXPECT_CALL(*service2.get(), Save(_)).WillOnce(Return(true));

  ASSERT_TRUE(profile_->AdoptService(service1));
  ASSERT_TRUE(profile_->AdoptService(service2));

  profile_->Save();
}

TEST_F(ProfileTest, EntryEnumeration) {
  scoped_refptr<MockService> service1(CreateMockService());
  scoped_refptr<MockService> service2(CreateMockService());
  string service1_storage_name = Technology::NameFromIdentifier(
      Technology::kCellular) + "_1";
  string service2_storage_name = Technology::NameFromIdentifier(
      Technology::kCellular) + "_2";
  EXPECT_CALL(*service1.get(), Save(_))
      .WillRepeatedly(Invoke(service1.get(), &MockService::FauxSave));
  EXPECT_CALL(*service2.get(), Save(_))
      .WillRepeatedly(Invoke(service2.get(), &MockService::FauxSave));
  EXPECT_CALL(*service1.get(), GetStorageIdentifier())
      .WillRepeatedly(Return(service1_storage_name));
  EXPECT_CALL(*service2.get(), GetStorageIdentifier())
      .WillRepeatedly(Return(service2_storage_name));

  string service1_name(service1->UniqueName());
  string service2_name(service2->UniqueName());

  ASSERT_TRUE(profile_->AdoptService(service1));
  ASSERT_TRUE(profile_->AdoptService(service2));

  Error error;
  ASSERT_EQ(2, profile_->EnumerateEntries(&error).size());

  ASSERT_TRUE(profile_->AbandonService(service1));
  ASSERT_EQ(service2_storage_name, profile_->EnumerateEntries(&error)[0]);

  ASSERT_TRUE(profile_->AbandonService(service2));
  ASSERT_EQ(0, profile_->EnumerateEntries(&error).size());
}

TEST_F(ProfileTest, MatchesIdentifier) {
  static const char kUser[] = "theUser";
  static const char kIdentifier[] = "theIdentifier";
  Profile::Identifier id(kUser, kIdentifier);
  ProfileRefPtr profile(
      new Profile(control_interface(), manager(), id, "", false));
  EXPECT_TRUE(profile->MatchesIdentifier(id));
  EXPECT_FALSE(profile->MatchesIdentifier(Profile::Identifier(kUser, "")));
  EXPECT_FALSE(
      profile->MatchesIdentifier(Profile::Identifier("", kIdentifier)));
  EXPECT_FALSE(
      profile->MatchesIdentifier(Profile::Identifier(kIdentifier, kUser)));
}

TEST_F(ProfileTest, InitStorage) {
  Profile::Identifier id("theUser", "theIdentifier");

  // Profile doesn't exist but we wanted it to.
  EXPECT_FALSE(ProfileInitStorage(id, Profile::kOpenExisting, false,
                                  Error::kNotFound));

  // Success case, with a side effect of creating the profile.
  EXPECT_TRUE(ProfileInitStorage(id, Profile::kCreateNew, true,
                                 Error::kSuccess));

  // The results from our two test cases above will now invert since
  // the profile now exists.  First, we now succeed if we require that
  // the profile already exist...
  EXPECT_TRUE(ProfileInitStorage(id, Profile::kOpenExisting, false,
                                 Error::kSuccess));

  // And we fail if we require that it doesn't.
  EXPECT_FALSE(ProfileInitStorage(id, Profile::kCreateNew, false,
                                  Error::kAlreadyExists));

  // As a sanity check, ensure "create or open" works for both profile-exists...
  EXPECT_TRUE(ProfileInitStorage(id, Profile::kCreateOrOpenExisting, false,
                                 Error::kSuccess));

  // ...and for a new profile that doesn't exist.
  Profile::Identifier id2("theUser", "theIdentifier2");
  // Let's just make double-check that this profile really doesn't exist.
  ASSERT_FALSE(ProfileInitStorage(id2, Profile::kOpenExisting, false,
                                  Error::kNotFound));

  // Then test that with "create or open" we succeed.
  EXPECT_TRUE(ProfileInitStorage(id2, Profile::kCreateOrOpenExisting, false,
                                 Error::kSuccess));
}

}  // namespace shill
