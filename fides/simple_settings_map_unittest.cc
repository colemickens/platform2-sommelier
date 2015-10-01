// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/simple_settings_map.h"

#include <base/logging.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "fides/identifier_utils.h"
#include "fides/mock_settings_document.h"
#include "fides/test_helpers.h"

namespace fides {

class SimpleSettingsMapTest : public testing::Test {
 public:
  SimpleSettingsMapTest() {
    // Prepare document for writer A.
    VersionStamp version_stamp_A;
    version_stamp_A.Set("A", 1);
    version_stamp_A.Set("B", 1);
    document_A_.reset(new MockSettingsDocument(version_stamp_A));

    // Prepare Document for writer B.
    VersionStamp version_stamp_B;
    version_stamp_B.Set("A", 2);
    version_stamp_B.Set("B", 1);
    document_B_.reset(new MockSettingsDocument(version_stamp_B));

    // Prepare Document for writer C.
    VersionStamp version_stamp_C;
    version_stamp_C.Set("A", 3);
    version_stamp_C.Set("B", 1);
    document_C_.reset(new MockSettingsDocument(version_stamp_C));

    // Prepare Document for writer D (concurrent to C).
    document_D_.reset(new MockSettingsDocument(version_stamp_C));
  }

  void CheckSettingsMapContents(
      const std::map<Key, std::string>& expected_values,
      const std::set<Key> expected_deletions,
      const SimpleSettingsMap& settings_map) {
    std::set<Key> value_keys = settings_map.GetKeys(Key());

    std::set<Key> expected_value_keys;
    for (auto& expected_value : expected_values) {
      expected_value_keys.insert(expected_value.first);
      BlobRef value = settings_map.GetValue(expected_value.first);
      EXPECT_TRUE(BlobRef(&expected_value.second).Equals(value))
          << "Unexpected value for key " << expected_value.first.ToString();
    }
    EXPECT_EQ(expected_value_keys, value_keys);

    std::set<Key> actual_deletions;
    for (auto& deletion : settings_map.deletion_map_) {
      actual_deletions.insert(deletion.first);
    }
    EXPECT_EQ(expected_deletions, actual_deletions);
  }

 protected:
  std::unique_ptr<MockSettingsDocument> document_A_;
  std::unique_ptr<MockSettingsDocument> document_B_;
  std::unique_ptr<MockSettingsDocument> document_C_;
  std::unique_ptr<MockSettingsDocument> document_D_;
};

TEST_F(SimpleSettingsMapTest, InsertionSingleDocument) {
  document_A_->SetKey(Key("A.B.C"), "1");
  document_A_->SetDeletion(Key("A.B"));
  document_A_->SetDeletion(Key("B"));

  SimpleSettingsMap settings_map;
  std::set<Key> modified_keys;
  EXPECT_TRUE(
      settings_map.InsertDocument(document_A_.get(), &modified_keys, nullptr));
  std::set<Key> expected_modifications = {Key("A.B.C")};
  EXPECT_EQ(expected_modifications, modified_keys);

  std::set<Key> expected_deletions = {Key("B"), Key("A.B")};
  std::map<Key, std::string> expected_values{
      {Key("A.B.C"), "1"},
  };
  CheckSettingsMapContents(expected_values, expected_deletions, settings_map);
}

TEST_F(SimpleSettingsMapTest, InsertionTwoDocuments) {
  document_A_->SetKey(Key("A.B.C"), "1");
  document_A_->SetDeletion(Key("A.B"));
  document_A_->SetDeletion(Key("B"));
  document_A_->SetKey(Key("B.C"), "2");
  document_B_->SetKey(Key("B.C"), "3");
  document_B_->SetDeletion(Key("A"));

  SimpleSettingsMap settings_map;
  EXPECT_TRUE(settings_map.InsertDocument(document_A_.get(), nullptr, nullptr));
  std::set<Key> modified_keys;
  EXPECT_TRUE(
      settings_map.InsertDocument(document_B_.get(), &modified_keys, nullptr));
  std::set<Key> expected_modifications = {Key("A.B.C"), Key("B.C")};
  EXPECT_EQ(expected_modifications, modified_keys);

  std::set<Key> expected_deletions = {Key("A"), Key("B")};
  std::map<Key, std::string> expected_values{
      {Key("B.C"), "3"},
  };
  CheckSettingsMapContents(expected_values, expected_deletions, settings_map);
}

TEST_F(SimpleSettingsMapTest, InsertionTwoDocumentsInverseOrder) {
  document_A_->SetKey(Key("A.B.C"), "1");
  document_A_->SetDeletion(Key("A.B"));
  document_A_->SetDeletion(Key("B"));
  document_B_->SetKey(Key("B.C"), "2");
  document_B_->SetDeletion(Key("A"));

  SimpleSettingsMap settings_map;
  EXPECT_TRUE(settings_map.InsertDocument(document_B_.get(), nullptr, nullptr));
  std::set<Key> modified_keys;
  EXPECT_TRUE(
      settings_map.InsertDocument(document_A_.get(), &modified_keys, nullptr));
  std::set<Key> expected_modifications;
  EXPECT_EQ(expected_modifications, modified_keys);

  std::set<Key> expected_deletions = {Key("A"), Key("B")};
  std::map<Key, std::string> expected_values = {
      {Key("B.C"), "2"},
  };
  CheckSettingsMapContents(expected_values, expected_deletions, settings_map);
}

TEST_F(SimpleSettingsMapTest, DocumentRemoval) {
  document_A_->SetKey(Key("A"), "1");
  document_A_->SetKey(Key("B"), "2");
  document_B_->SetKey(Key("B"), "3");
  document_B_->SetKey(Key("C"), "4");

  SimpleSettingsMap settings_map;
  EXPECT_TRUE(settings_map.InsertDocument(document_A_.get(), nullptr, nullptr));
  EXPECT_TRUE(settings_map.InsertDocument(document_B_.get(), nullptr, nullptr));
  std::set<Key> modified_keys;
  settings_map.RemoveDocument(document_B_.get(), &modified_keys, nullptr);
  std::set<Key> expected_modifications = {Key("B"), Key("C")};
  EXPECT_EQ(expected_modifications, modified_keys);

  std::set<Key> expected_deletions = {};
  std::map<Key, std::string> expected_values{
      {Key("A"), "1"}, {Key("B"), "2"},
  };
  CheckSettingsMapContents(expected_values, expected_deletions, settings_map);
}

TEST_F(SimpleSettingsMapTest, RemovalOfDeletion) {
  document_A_->SetKey(Key("A"), "1");
  document_A_->SetKey(Key("B.C"), "2");
  document_B_->SetDeletion(Key("B"));

  SimpleSettingsMap settings_map;
  EXPECT_TRUE(settings_map.InsertDocument(document_A_.get(), nullptr, nullptr));
  EXPECT_TRUE(settings_map.InsertDocument(document_B_.get(), nullptr, nullptr));
  std::set<Key> modified_keys;
  settings_map.RemoveDocument(document_B_.get(), &modified_keys, nullptr);
  std::set<Key> expected_modifications = {Key("B.C")};
  EXPECT_EQ(expected_modifications, modified_keys);

  std::set<Key> expected_deletions = {};
  std::map<Key, std::string> expected_values = {
      {Key("A"), "1"}, {Key("B.C"), "2"},
  };
  CheckSettingsMapContents(expected_values, expected_deletions, settings_map);
}

TEST_F(SimpleSettingsMapTest, RemovalOfDeletionChildPrefixShineThrough) {
  document_A_->SetKey(Key("A.B.D"), "1");
  document_A_->SetKey(Key("Z.A"), "-1");
  document_B_->SetKey(Key("A.B.C"), "2");
  document_B_->SetKey(Key("Z.B"), "-1");
  document_C_->SetDeletion(Key("A.B"));

  SimpleSettingsMap settings_map;
  EXPECT_TRUE(settings_map.InsertDocument(document_A_.get(), nullptr, nullptr));
  EXPECT_TRUE(settings_map.InsertDocument(document_B_.get(), nullptr, nullptr));
  EXPECT_TRUE(settings_map.InsertDocument(document_C_.get(), nullptr, nullptr));
  std::set<Key> modified_keys;
  settings_map.RemoveDocument(document_C_.get(), &modified_keys, nullptr);
  std::set<Key> expected_modifications = {Key("A.B.C"), Key("A.B.D")};
  EXPECT_EQ(expected_modifications, modified_keys);

  std::set<Key> expected_deletions = {};
  std::map<Key, std::string> expected_values = {
      {Key("A.B.C"), "2"},
      {Key("A.B.D"), "1"},
      {Key("Z.A"), "-1"},
      {Key("Z.B"), "-1"},
  };
  CheckSettingsMapContents(expected_values, expected_deletions, settings_map);
}

TEST_F(SimpleSettingsMapTest, RemovalOfDeletionParentDeleterUpstream) {
  document_A_->SetKey(Key("A.A"), "1");
  document_A_->SetKey(Key("A.B.C"), "2");
  document_A_->SetKey(Key("Z.A"), "-1");
  document_B_->SetDeletion(Key("A"));
  document_B_->SetKey(Key("Z.B"), "-1");
  document_C_->SetDeletion(Key("A.B"));

  SimpleSettingsMap settings_map;
  EXPECT_TRUE(settings_map.InsertDocument(document_A_.get(), nullptr, nullptr));
  EXPECT_TRUE(settings_map.InsertDocument(document_B_.get(), nullptr, nullptr));
  EXPECT_TRUE(settings_map.InsertDocument(document_C_.get(), nullptr, nullptr));
  std::set<Key> modified_keys;
  settings_map.RemoveDocument(document_C_.get(), &modified_keys, nullptr);
  std::set<Key> expected_modifications;
  EXPECT_EQ(expected_modifications, modified_keys);

  std::set<Key> expected_deletions = {Key("A")};
  std::map<Key, std::string> expected_values = {
      {Key("Z.A"), "-1"}, {Key("Z.B"), "-1"},
  };
  CheckSettingsMapContents(expected_values, expected_deletions, settings_map);
}

TEST_F(SimpleSettingsMapTest, RemovalOfDeletionChildDeleterUpstream) {
  document_A_->SetKey(Key("A.B.C.D"), "1");
  document_A_->SetKey(Key("A.B.D"), "2");
  document_A_->SetKey(Key("Z.A"), "-1");
  document_B_->SetDeletion(Key("A.B.C"));
  document_B_->SetKey(Key("Z.B"), "-1");
  document_C_->SetDeletion(Key("A.B"));

  SimpleSettingsMap settings_map;
  EXPECT_TRUE(settings_map.InsertDocument(document_A_.get(), nullptr, nullptr));
  EXPECT_TRUE(settings_map.InsertDocument(document_B_.get(), nullptr, nullptr));
  EXPECT_TRUE(settings_map.InsertDocument(document_C_.get(), nullptr, nullptr));
  std::set<Key> modified_keys;
  settings_map.RemoveDocument(document_C_.get(), &modified_keys, nullptr);
  std::set<Key> expected_modifications = {Key("A.B.D")};
  EXPECT_EQ(expected_modifications, modified_keys);

  std::set<Key> expected_deletions = {Key("A.B.C")};
  std::map<Key, std::string> expected_values = {
      {Key("A.B.D"), "2"},
      {Key("Z.A"), "-1"},
      {Key("Z.B"), "-1"},
  };
  CheckSettingsMapContents(expected_values, expected_deletions, settings_map);
}

TEST_F(SimpleSettingsMapTest, BasicRemovalOfDeletionSameDeletionUpstream) {
  document_A_->SetKey(Key("A.B.C.D"), "1");
  document_A_->SetKey(Key("A.B.D"), "2");
  document_A_->SetKey(Key("Z.A"), "-1");
  document_B_->SetDeletion(Key("A.B"));
  document_B_->SetKey(Key("A.B.C"), "3");
  document_B_->SetKey(Key("Z.B"), "-1");
  document_C_->SetDeletion(Key("A.B"));

  SimpleSettingsMap settings_map;
  EXPECT_TRUE(settings_map.InsertDocument(document_A_.get(), nullptr, nullptr));
  EXPECT_TRUE(settings_map.InsertDocument(document_B_.get(), nullptr, nullptr));
  EXPECT_TRUE(settings_map.InsertDocument(document_C_.get(), nullptr, nullptr));
  std::set<Key> modified_keys;
  settings_map.RemoveDocument(document_C_.get(), &modified_keys, nullptr);
  std::set<Key> expected_modifications = {Key("A.B.C")};
  EXPECT_EQ(expected_modifications, modified_keys);

  std::set<Key> expected_deletions = {Key("A.B")};
  std::map<Key, std::string> expected_values = {
      {Key("A.B.C"), "3"},
      {Key("Z.A"), "-1"},
      {Key("Z.B"), "-1"},
  };
  CheckSettingsMapContents(expected_values, expected_deletions, settings_map);
}

TEST_F(SimpleSettingsMapTest, DocumentCollision) {
  document_C_->SetKey(Key("A.B.C.D"), "2");
  document_D_->SetKey(Key("A.B.C.D"), "3");

  SimpleSettingsMap settings_map;
  EXPECT_TRUE(settings_map.InsertDocument(document_C_.get(), nullptr, nullptr));
  std::set<Key> modified_keys;
  EXPECT_FALSE(
      settings_map.InsertDocument(document_D_.get(), &modified_keys, nullptr));
  std::set<Key> expected_modifications;
  EXPECT_EQ(expected_modifications, modified_keys);

  std::set<Key> expected_deletions;
  std::map<Key, std::string> expected_values = {
      {Key("A.B.C.D"), "2"},
  };
  CheckSettingsMapContents(expected_values, expected_deletions, settings_map);
}

TEST_F(SimpleSettingsMapTest, InsertEmptyDocument) {
  SimpleSettingsMap settings_map;
  std::vector<const SettingsDocument*> unreferenced_documents;
  EXPECT_TRUE(settings_map.InsertDocument(document_A_.get(), nullptr,
                                          &unreferenced_documents));

  std::vector<const SettingsDocument*> expected_unreferenced_documents = {
      document_A_.get()
  };
  EXPECT_EQ(expected_unreferenced_documents, unreferenced_documents);
}

TEST_F(SimpleSettingsMapTest, UnreferencedDocsOverwrite) {
  document_A_->SetKey(Key("A"), "1");
  document_B_->SetKey(Key("A"), "2");

  SimpleSettingsMap settings_map;
  std::set<Key> modified_keys;
  std::vector<const SettingsDocument*> unreferenced_documents;

  EXPECT_TRUE(settings_map.InsertDocument(document_A_.get(), &modified_keys,
                                          &unreferenced_documents));
  std::set<Key> expected_modified_keys = {Key("A")};
  std::vector<const SettingsDocument*> expected_unreferenced_documents;
  EXPECT_EQ(expected_modified_keys, modified_keys);
  EXPECT_EQ(expected_unreferenced_documents, unreferenced_documents);

  EXPECT_TRUE(settings_map.InsertDocument(document_B_.get(), &modified_keys,
                                          &unreferenced_documents));
  expected_modified_keys = {Key("A")};
  expected_unreferenced_documents = {document_A_.get()};
  EXPECT_EQ(expected_modified_keys, modified_keys);
  EXPECT_EQ(expected_unreferenced_documents, unreferenced_documents);
}

TEST_F(SimpleSettingsMapTest, UnreferencedDocsDeletion) {
  document_A_->SetKey(Key("A.B"), "1");
  document_B_->SetDeletion(Key("A"));

  SimpleSettingsMap settings_map;
  std::set<Key> modified_keys;
  std::vector<const SettingsDocument*> unreferenced_documents;

  EXPECT_TRUE(settings_map.InsertDocument(document_A_.get(), &modified_keys,
                                          &unreferenced_documents));
  std::set<Key> expected_modified_keys = {Key("A.B")};
  std::vector<const SettingsDocument*> expected_unreferenced_documents;
  EXPECT_EQ(expected_modified_keys, modified_keys);
  EXPECT_EQ(expected_unreferenced_documents, unreferenced_documents);

  EXPECT_TRUE(settings_map.InsertDocument(document_B_.get(), &modified_keys,
                                          &unreferenced_documents));
  expected_modified_keys = {Key("A.B")};
  expected_unreferenced_documents = {document_A_.get()};
  EXPECT_EQ(expected_modified_keys, modified_keys);
  EXPECT_EQ(expected_unreferenced_documents, unreferenced_documents);
}

}  // namespace fides
