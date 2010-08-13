// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/pref_store.h"

#include <base/file_util.h>
#include <base/file_path.h>
#include <base/logging.h>
#include <base/scoped_temp_dir.h>
#include <gtest/gtest.h>

namespace login_manager {
struct TestPrefsData {
  const char* name;
  const char* value;
  const char* signature;
};

struct TestWhitelistData {
  const char* email;
  const char* signature;
};

const TestPrefsData kDefaultPrefs[] = {
  {"zero", "foo", "foo_sig"},
  {"one", "boo", "boo_sig"},
  {"two", "goo", "goo_sig"},
};

const TestWhitelistData kDefaultUsers[] = {
   {"testuser0@invalid.domain", "zero_sig"},
   {"testuser1@invalid.domain", "one_sig"},
   {"testuser2@invalid.domain", "two_sig"},
};

class PrefStoreTest : public ::testing::Test {
 public:
  PrefStoreTest() {}

  virtual ~PrefStoreTest() {}

  bool StartFresh() {
    return file_util::Delete(tmpfile_, false);
  }

  virtual void SetUp() {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());

    // Create a temporary filename that's guaranteed to not exist, but is
    // inside our scoped directory so it'll get deleted later.
    ASSERT_TRUE(file_util::CreateTemporaryFileInDir(tmpdir_.path(), &tmpfile_));
    ASSERT_TRUE(StartFresh());

    // Dump some test data into the file.
    store_.reset(new PrefStore(tmpfile_));
    ASSERT_TRUE(store_->LoadOrCreate());

    store_->Set(kDefaultPrefs[0].name,
                kDefaultPrefs[0].value,
                kDefaultPrefs[0].signature);
    store_->Set(kDefaultPrefs[1].name,
                kDefaultPrefs[1].value,
                kDefaultPrefs[1].signature);
    store_->Set(kDefaultPrefs[2].name,
                kDefaultPrefs[2].value,
                kDefaultPrefs[2].signature);

    store_->Whitelist(kDefaultUsers[0].email,
                      kDefaultUsers[0].signature);
    store_->Whitelist(kDefaultUsers[1].email,
                      kDefaultUsers[1].signature);

    ASSERT_TRUE(store_->Persist());
  }

  virtual void TearDown() {
  }

  void CheckExpectedPrefs(PrefStore* store) {
    std::string tmp_value;
    std::string tmp_sig;

    EXPECT_TRUE(store->Get(kDefaultPrefs[0].name, &tmp_value, &tmp_sig));
    EXPECT_EQ(kDefaultPrefs[0].value, tmp_value);
    EXPECT_EQ(kDefaultPrefs[0].signature, tmp_sig);

    EXPECT_TRUE(store->Get(kDefaultPrefs[1].name, &tmp_value, &tmp_sig));
    EXPECT_EQ(kDefaultPrefs[1].value, tmp_value);
    EXPECT_EQ(kDefaultPrefs[1].signature, tmp_sig);

    EXPECT_TRUE(store->Get(kDefaultPrefs[2].name, &tmp_value, &tmp_sig));
    EXPECT_EQ(kDefaultPrefs[2].value, tmp_value);
    EXPECT_EQ(kDefaultPrefs[2].signature, tmp_sig);
  }

  void CheckExpectedWhitelist(PrefStore* store) {
    std::string tmp_sig;

    EXPECT_TRUE(store->GetFromWhitelist(kDefaultUsers[0].email, &tmp_sig));
    EXPECT_EQ(kDefaultUsers[0].signature, tmp_sig);

    EXPECT_TRUE(store->GetFromWhitelist(kDefaultUsers[1].email, &tmp_sig));
    EXPECT_EQ(kDefaultUsers[1].signature, tmp_sig);
  }

  ScopedTempDir tmpdir_;
  FilePath tmpfile_;
  scoped_ptr<PrefStore> store_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefStoreTest);
};

TEST_F(PrefStoreTest, CreateEmptyStore) {
  StartFresh();
  PrefStore store(tmpfile_);
  ASSERT_TRUE(store.LoadOrCreate());  // Should create an empty DictionaryValue.
}

TEST_F(PrefStoreTest, FailBrokenStore) {
  FilePath bad_file;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(tmpdir_.path(), &bad_file));
  PrefStore store(bad_file);
  ASSERT_FALSE(store.LoadOrCreate());
}

TEST_F(PrefStoreTest, VerifyPrefStorage) {
  CheckExpectedPrefs(store_.get());
}

TEST_F(PrefStoreTest, VerifyPrefRemove) {
  std::string tmp_value;
  std::string tmp_sig;

  ASSERT_TRUE(store_->Get(kDefaultPrefs[0].name, &tmp_value, &tmp_sig));
  ASSERT_EQ(kDefaultPrefs[0].value, tmp_value);
  ASSERT_EQ(kDefaultPrefs[0].signature, tmp_sig);

  tmp_value.clear();
  tmp_sig.clear();
  ASSERT_TRUE(store_->Remove(kDefaultPrefs[0].name, &tmp_value, &tmp_sig));
  ASSERT_EQ(kDefaultPrefs[0].value, tmp_value);
  ASSERT_EQ(kDefaultPrefs[0].signature, tmp_sig);

  tmp_value.clear();
  tmp_sig.clear();
  ASSERT_FALSE(store_->Get(kDefaultPrefs[0].name, &tmp_value, &tmp_sig));
}

TEST_F(PrefStoreTest, VerifyPrefDelete) {
  std::string tmp_value;
  std::string tmp_sig;

  ASSERT_TRUE(store_->Get(kDefaultPrefs[0].name, &tmp_value, &tmp_sig));
  ASSERT_EQ(kDefaultPrefs[0].value, tmp_value);
  ASSERT_EQ(kDefaultPrefs[0].signature, tmp_sig);

  store_->Delete(kDefaultPrefs[0].name);

  tmp_value.clear();
  tmp_sig.clear();
  ASSERT_FALSE(store_->Get(kDefaultPrefs[0].name, &tmp_value, &tmp_sig));
}

TEST_F(PrefStoreTest, VerifyWhitelistStorage) {
  CheckExpectedWhitelist(store_.get());
}

TEST_F(PrefStoreTest, VerifyUnwhitelist) {
  std::string tmp_sig;

  ASSERT_TRUE(store_->GetFromWhitelist(kDefaultUsers[0].email, &tmp_sig));
  ASSERT_EQ(kDefaultUsers[0].signature, tmp_sig);

  store_->Unwhitelist(kDefaultUsers[0].email);

  tmp_sig.clear();
  ASSERT_FALSE(store_->GetFromWhitelist(kDefaultUsers[0].email, &tmp_sig));
}

TEST_F(PrefStoreTest, LoadStoreFromDisk) {
  PrefStore store2(tmpfile_);
  ASSERT_TRUE(store2.LoadOrCreate());
  CheckExpectedPrefs(&store2);
  CheckExpectedWhitelist(&store2);
}

}  // namespace login_manager
