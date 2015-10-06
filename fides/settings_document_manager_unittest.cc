// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/settings_document_manager.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>

#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "fides/mock_locked_settings.h"
#include "fides/mock_settings_document.h"
#include "fides/settings_keys.h"
#include "fides/simple_settings_map.h"
#include "fides/source.h"
#include "fides/source_delegate.h"
#include "fides/test_helpers.h"
#include "fides/version_stamp.h"

namespace fides {

namespace {

// Test constants.
const char kTestSource0[] = "source0";
const char kTestSource1[] = "source1";
const char kTestSource2[] = "source2";

const char kSharedKey[] = "shared";

// A mock settings blob parser that returns staged LockedSettingsContainer
// instances.
class MockSettingsBlobParser {
 public:
  MockSettingsBlobParser() = default;

  // Parse a blob. The corresponding LockedSettingsContainer instance must have
  // been registered with the parser via Register().
  std::unique_ptr<LockedSettingsContainer> operator()(const std::string& format,
                                                      BlobRef data) {
    if (!data.valid())
      return nullptr;
    auto entry = containers_.find(data.ToString());
    if (entry != containers_.end())
      return entry->second->Clone();
    return nullptr;
  }

  // Registers a LockedSettingsContainer with the parser. Returns the BlobRef
  // that'll make the parser return the container.
  BlobRef Register(MockLockedSettingsContainer* container) {
    const std::string blob_id = "blob_" + std::to_string(next_blob_id_++);
    containers_[blob_id] = container;
    return BlobRef(&containers_.find(blob_id)->first);
  }

  // Unregisters a LockedSettingsContainer with the parser.
  void Unregister(MockLockedSettingsContainer* container) {
    for (auto it = containers_.cbegin(); it != containers_.cend();) {
      if (it->second == container) {
        containers_.erase(it++);
      } else {
        ++it;
      }
    }
  }

 private:
  int next_blob_id_ = 0;
  std::unordered_map<std::string, MockLockedSettingsContainer*> containers_;

  DISALLOW_COPY_AND_ASSIGN(MockSettingsBlobParser);
};

// A SourceDelegate implementation with behavior configurable at construction
// time.
class MockSourceDelegate : public SourceDelegate {
 public:
  MockSourceDelegate() {}
  ~MockSourceDelegate() override = default;

  // SourceDelegate:
  bool ValidateVersionComponent(
      const LockedVersionComponent& version_component) const override {
    return static_cast<const MockLockedVersionComponent&>(version_component)
        .is_valid();
  }
  bool ValidateContainer(
      const LockedSettingsContainer& container) const override {
    return static_cast<const MockLockedSettingsContainer&>(container)
        .is_valid();
  }

  // This function is suitable as a SourceDelegateFactoryFunction.
  static std::unique_ptr<SourceDelegate> Create(
      const std::string& source_id,
      const SettingsService& settings) {
    return std::unique_ptr<SourceDelegate>(new MockSourceDelegate);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSourceDelegate);
};

// A SettingsObserver implementation that verifies expectations.
class SettingsChangeVerifier : public SettingsObserver {
 public:
  SettingsChangeVerifier(SettingsDocumentManager* manager,
                         const std::deque<std::set<Key>>& expectations)
      : manager_(manager), expectations_(expectations) {
    manager_->AddSettingsObserver(this);
  }

  ~SettingsChangeVerifier() {
    EXPECT_TRUE(expectations_.empty());
    manager_->RemoveSettingsObserver(this);
  }

  void OnSettingsChanged(const std::set<Key>& keys) override {
    ASSERT_FALSE(expectations_.empty());
    EXPECT_TRUE(std::includes(keys.begin(), keys.end(),
                              expectations_.front().begin(),
                              expectations_.front().end()));
    expectations_.pop_front();
  }

 private:
  SettingsDocumentManager* manager_;
  std::deque<std::set<Key>> expectations_;

  DISALLOW_COPY_AND_ASSIGN(SettingsChangeVerifier);
};

// Add trust configuration for |source_id| to |doc|.
static void ConfigureSource(MockSettingsDocument* doc,
                            const std::string& source_id,
                            SettingStatus status,
                            const std::map<Key, SettingStatus> access_rules) {
  doc->SetKey(MakeSourceKey(source_id).Extend({keys::sources::kStatus}),
              SettingStatusToString(status));
  doc->SetKey(MakeSourceKey(source_id).Extend({keys::sources::kName}),
              source_id);
  doc->SetKey(MakeSourceKey(source_id).Extend({keys::sources::kType}),
              source_id);
  for (auto& access_rule : access_rules) {
    doc->SetKey(MakeSourceKey(source_id)
                    .Extend({keys::sources::kAccess})
                    .Append(access_rule.first),
                SettingStatusToString(access_rule.second));
  }
}

// Create the initial trusted settings document for bootstrapping
// SettingsDocumentManager. This configures a source that has access to its
// sentinel value as well as the trust configuration for other sources.
std::unique_ptr<const SettingsDocument> CreateInitialTrustedSettingsDocument() {
  // No version stamp is fine, since the initial document can't collide with
  // anything and doesn't have an associated source for which it'd need to
  // supply a unique version component.
  std::unique_ptr<MockSettingsDocument> document(
      new MockSettingsDocument(VersionStamp()));
  ConfigureSource(document.get(), kTestSource0, kSettingStatusActive,
                  {{Key(kTestSource0), kSettingStatusActive},
                   {MakeSourceKey(kTestSource1), kSettingStatusActive},
                   {MakeSourceKey(kTestSource2), kSettingStatusActive}});
  return std::move(document);
}

}  // namespace

class SettingsDocumentManagerTest : public testing::Test {
 public:
  SettingsDocumentManagerTest() {
    // Create permissive source delegates for the test sources.
    for (auto& source : {kTestSource0, kTestSource1, kTestSource2}) {
      source_delegate_factory_.RegisterFunction(source,
                                                MockSourceDelegate::Create);
    }
  }

  void SetUp() override {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
    ReinitializeSettingsDocumentManager();
  }

  void ReinitializeSettingsDocumentManager() {
    manager_.reset(new SettingsDocumentManager(
        std::ref(parser_), std::ref(source_delegate_factory_),
        tmpdir_.path().value(),
        std::unique_ptr<SettingsMap>(new SimpleSettingsMap()),
        CreateInitialTrustedSettingsDocument()));
    manager_->Init();
  }

  // Creates a container and moves |payload| inside.
  MockLockedSettingsContainer* MakeContainer(
      std::unique_ptr<MockSettingsDocument> payload) {
    std::unique_ptr<MockLockedSettingsContainer> container(
        new MockLockedSettingsContainer(std::move(payload)));
    MockLockedSettingsContainer* container_ptr = container.get();
    containers_.push_back(std::move(container));
    return container_ptr;
  }

  SettingsDocumentManager::InsertionStatus InsertBlob(
      const std::string& source_id,
      BlobRef blob,
      const std::deque<std::set<Key>>& expected_changes) {
    SettingsChangeVerifier verifier(manager_.get(), expected_changes);
    return manager_->InsertBlob(source_id, blob);
  }

  // Creates a settings document with a bumped version stamp.
  std::unique_ptr<MockSettingsDocument> MakeDocument(
      const std::string& source_id) {
    current_version_.Set(source_id, current_version_.Get(source_id) + 1);
    return std::unique_ptr<MockSettingsDocument>(
        new MockSettingsDocument(current_version_));
  }

  // Convenience wrapper for SettingsDocumentManager::InsertBlob, which wraps
  // the given |document| in a MockLockedSettingsContainer, registers the
  // container with the parser, and inserts the resulting blob. This also makes
  // sure that the parser can re-load the document for re-validation. This
  // function is useful for test cases that just want to insert a document -
  // call InsertBlob directly for test cases that exercise blob insertion logic.
  SettingsDocumentManager::InsertionStatus InsertDocument(
      std::unique_ptr<MockSettingsDocument> document,
      const std::string& source_id,
      const std::deque<std::set<Key>>& expected_changes) {
    SettingsChangeVerifier verifier(manager_.get(), expected_changes);
    return manager_->InsertBlob(
        source_id, parser_.Register(MakeContainer(std::move(document))));
  }

  // A helper to configure a trusted source.
  void ConfigureTrustedSource(const std::string& added_source) {
    SCOPED_TRACE("Configuring source " + added_source);
    std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource0));
    ConfigureSource(document.get(), added_source, kSettingStatusActive,
                    {{Key(added_source), kSettingStatusActive},
                     {Key(kSharedKey), kSettingStatusActive}});
    EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
              InsertDocument(std::move(document), kTestSource0, {{}}));
  }

  // A helper that sets a settings key serving as the sentinel value for whether
  // the source it originates from is still valid.
  MockLockedSettingsContainer* AddSentinelValue(const std::string& source_id) {
    SCOPED_TRACE("Adding sentinel value for " + source_id);
    std::unique_ptr<MockSettingsDocument> document(MakeDocument(source_id));
    document->SetKey(Key(source_id), source_id);
    std::unique_ptr<MockLockedSettingsContainer> container(
        new MockLockedSettingsContainer(std::move(document)));
    MockLockedSettingsContainer* container_ptr = container.get();
    containers_.push_back(std::move(container));
    BlobRef blob_ref = parser_.Register(container_ptr);
    EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
              InsertBlob(source_id, blob_ref, {{}}));
    return container_ptr;
  }

  // Checks presence of the correct sentinel values.
  void CheckSentinelValues(std::initializer_list<std::string> present,
                           std::initializer_list<std::string> absent) {
    for (auto& source : present) {
      BlobRef value = manager_->GetValue(Key(source));
      EXPECT_TRUE(value.valid()) << "Sentinel value " << source << " missing.";
      EXPECT_EQ(source, value.ToString()) << "Sentinel value " << source
                                          << " has wrong value.";
    }

    for (auto& source : absent) {
      BlobRef value = manager_->GetValue(Key(source));
      EXPECT_FALSE(value.valid()) << "Sentinel value " << source << " present.";
    }
  }

  // Changes the trust of |source_id|'s sentinel value for |source_id| to
  // |status|.
  void SetTrustForSentinelKey(const std::string& source_id,
                              SettingStatus status) {
    std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource0));
    document->SetKey(MakeSourceKey(source_id)
                         .Extend({keys::sources::kAccess})
                         .Append(Key(source_id)),
                     SettingStatusToString(status));
    EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
              InsertDocument(std::move(document), kTestSource0, {{}}));
  }

  base::ScopedTempDir tmpdir_;
  std::vector<std::unique_ptr<LockedSettingsContainer>> containers_;
  MockSettingsBlobParser parser_;
  SourceDelegateFactory source_delegate_factory_;
  VersionStamp current_version_;
  std::unique_ptr<SettingsDocumentManager> manager_;
};

TEST_F(SettingsDocumentManagerTest, ValueInsertionAndRemoval) {
  ConfigureTrustedSource(kTestSource1);
  const Key kTestKey(kTestSource1);

  // Insert a document with a fresh key.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource1));
  document->SetKey(kTestKey, "42");
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource1, {{kTestKey}}));
  BlobRef value = manager_->GetValue(kTestKey);
  EXPECT_TRUE(value.valid());
  EXPECT_EQ("42", value.ToString());

  // Update the value.
  document = MakeDocument(kTestSource1);
  document->SetKey(kTestKey, "string");
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource1, {{kTestKey}}));
  BlobRef new_value = manager_->GetValue(kTestKey);
  EXPECT_TRUE(new_value.valid());
  EXPECT_EQ("string", new_value.ToString());

  // Clear the value.
  document = MakeDocument(kTestSource1);
  document->SetDeletion(kTestKey);
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource1, {{kTestKey}}));
  BlobRef newer_value = manager_->GetValue(kTestKey);
  EXPECT_FALSE(newer_value.valid());
}

TEST_F(SettingsDocumentManagerTest, TrustChange) {
  ConfigureTrustedSource(kTestSource1);
  AddSentinelValue(kTestSource1);

  // Check whether the sentinel for the inserted source is present.
  CheckSentinelValues({kTestSource1}, {});

  // Remove trust, which should make the sentinel value disappear.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource0));
  ConfigureSource(document.get(), kTestSource1, kSettingStatusInvalid, {});
  EXPECT_EQ(
      SettingsDocumentManager::kInsertionStatusSuccess,
      InsertDocument(std::move(document), kTestSource0, {{Key(kTestSource1)}}));
  CheckSentinelValues({}, {kTestSource1});
}

TEST_F(SettingsDocumentManagerTest, CascadingRemoval) {
  // Have source0 add source1 and grant it access to source2.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource0));
  ConfigureSource(document.get(), kTestSource1, kSettingStatusActive,
                  {{Key(kTestSource1), kSettingStatusActive},
                   {MakeSourceKey(kTestSource2), kSettingStatusActive}});
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource0, {{}}));
  AddSentinelValue(kTestSource1);

  // Have source1 extend trust to source2.
  document = MakeDocument(kTestSource1);
  ConfigureSource(document.get(), kTestSource2, kSettingStatusActive,
                  {{Key(kTestSource2), kSettingStatusActive}});
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource1, {{}}));
  AddSentinelValue(kTestSource2);

  // Both sentinels should be present.
  CheckSentinelValues({kTestSource1, kTestSource2}, {});

  // Revoke trust from kTestSource1. kTestSource2 becomes invalid as well.
  document = MakeDocument(kTestSource0);
  ConfigureSource(document.get(), kTestSource1, kSettingStatusInvalid, {});
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource0,
                           {{Key(kTestSource1), Key(kTestSource2)}}));
  CheckSentinelValues({}, {kTestSource1, kTestSource2});
}

TEST_F(SettingsDocumentManagerTest, TrustChangeDeletion) {
  ConfigureTrustedSource(kTestSource1);
  AddSentinelValue(kTestSource1);
  CheckSentinelValues({kTestSource1}, {});

  // Remove trust via a deletion. The sentinel value should disappear.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource0));
  document->SetDeletion(Key(MakeSourceKey(kTestSource1)));
  EXPECT_EQ(
      SettingsDocumentManager::kInsertionStatusSuccess,
      InsertDocument(std::move(document), kTestSource0, {{Key(kTestSource1)}}));
  CheckSentinelValues({}, {kTestSource1});
}

TEST_F(SettingsDocumentManagerTest, TrustChangeAccessRules) {
  ConfigureTrustedSource(kTestSource1);
  AddSentinelValue(kTestSource1);
  CheckSentinelValues({kTestSource1}, {});

  // Revoke source1's ability to write its sentinel.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource0));
  document->SetKey(
      MakeSourceKey(kTestSource1)
          .Extend({keys::sources::kAccess, kTestSource1}),
      SettingStatusToString(kSettingStatusInvalid));
  EXPECT_EQ(
      SettingsDocumentManager::kInsertionStatusSuccess,
      InsertDocument(std::move(document), kTestSource0, {{Key(kTestSource1)}}));
  CheckSentinelValues({}, {kTestSource1});
}

TEST_F(SettingsDocumentManagerTest, TrustChangeWithdrawnSource) {
  ConfigureTrustedSource(kTestSource1);
  AddSentinelValue(kTestSource1);
  CheckSentinelValues({kTestSource1}, {});

  // Switch the source to withdrawn state.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource0));
  ConfigureSource(document.get(), kTestSource1, kSettingStatusWithdrawn, {});
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource0, {{}}));

  // The value should still remain present.
  CheckSentinelValues({kTestSource1}, {});

  // source1 may no longer change the value.
  document = MakeDocument(kTestSource1);
  document->SetKey(Key(kTestSource1), "change");
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusAccessViolation,
            InsertDocument(std::move(document), kTestSource1, {}));
}

TEST_F(SettingsDocumentManagerTest, TrustChangeWithdrawnAccessRules) {
  ConfigureTrustedSource(kTestSource1);
  AddSentinelValue(kTestSource1);
  CheckSentinelValues({kTestSource1}, {});

  // Change access rule for the sentinel key to withdrawn.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource0));
  ConfigureSource(document.get(), kTestSource1, kSettingStatusActive,
                  {{Key(kTestSource1), kSettingStatusWithdrawn}});
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource0, {{}}));

  // The value should still remain present.
  CheckSentinelValues({kTestSource1}, {});

  // source1 may no longer change the value.
  document = MakeDocument(kTestSource1);
  document->SetKey(Key(kTestSource1), "change");
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusAccessViolation,
            InsertDocument(std::move(document), kTestSource1, {}));
}

TEST_F(SettingsDocumentManagerTest, InsertionFailureStatus) {
  // Configure source1, but in invalid state.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource0));
  ConfigureSource(document.get(), kTestSource1, kSettingStatusInvalid,
                  {{Key(kTestSource1), kSettingStatusActive}});
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource0, {{}}));

  // Inserting a document should fail.
  document = MakeDocument(kTestSource1);
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusAccessViolation,
            InsertDocument(std::move(document), kTestSource1, {}));
}

TEST_F(SettingsDocumentManagerTest, InsertionFailureAccessRules) {
  ConfigureTrustedSource(kTestSource1);

  // Inserting a document with a key the source can't write to should fail.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource1));
  document->SetKey(Key("A"), "42");
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusAccessViolation,
            InsertDocument(std::move(document), kTestSource1, {}));
}

TEST_F(SettingsDocumentManagerTest, InsertionFailureVersionClash) {
  ConfigureTrustedSource(kTestSource1);
  AddSentinelValue(kTestSource1);

  // Inserting a document with an already-used version stamp component for the
  // issuing source should fail, even if there is no collision.
  VersionStamp version_stamp = current_version_;
  version_stamp.Set(kTestSource2, current_version_.Get(kTestSource2) + 1);
  EXPECT_TRUE(version_stamp.IsAfter(current_version_));
  std::unique_ptr<MockSettingsDocument> document(
      new MockSettingsDocument(version_stamp));
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusVersionClash,
            InsertDocument(std::move(document), kTestSource1, {}));
}

TEST_F(SettingsDocumentManagerTest, InsertionFailureVersionCollision) {
  ConfigureTrustedSource(kTestSource1);
  ConfigureTrustedSource(kTestSource2);

  VersionStamp initial_version = current_version_;

  // Insert a value for |kSharedKey|.
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource1));
  document->SetKey(Key(kSharedKey), "42");
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertDocument(std::move(document), kTestSource1, {{}}));

  // Construct a colliding document, which should fail insertion.
  VersionStamp previous_version(current_version_);
  current_version_ = initial_version;
  document = MakeDocument(kTestSource2);
  document->SetKey(Key(kSharedKey), "0");
  EXPECT_TRUE(previous_version.IsConcurrent(current_version_));
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusCollision,
            InsertDocument(std::move(document), kTestSource2, {}));
}

TEST_F(SettingsDocumentManagerTest, InsertBlobSuccess) {
  ConfigureTrustedSource(kTestSource1);
  std::unique_ptr<MockSettingsDocument> document(MakeDocument(kTestSource1));
  document->SetKey(Key(kTestSource1), kTestSource1);
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusSuccess,
            InsertBlob(kTestSource1,
                       parser_.Register(MakeContainer(std::move(document))),
                       {{Key(kTestSource1)}}));
  CheckSentinelValues({kTestSource1}, {});
}

TEST_F(SettingsDocumentManagerTest, InsertBlobUnknownSource) {
  EXPECT_EQ(
      SettingsDocumentManager::kInsertionStatusUnknownSource,
      InsertBlob(kTestSource1,
                 parser_.Register(MakeContainer(MakeDocument(kTestSource1))),
                 {}));
}

TEST_F(SettingsDocumentManagerTest, InsertBlobParseError) {
  ConfigureTrustedSource(kTestSource1);
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusParseError,
            InsertBlob(kTestSource1, BlobRef(nullptr, 0), {}));
}

TEST_F(SettingsDocumentManagerTest, InsertBlobValidationErrorNoDelegate) {
  source_delegate_factory_.RegisterFunction(
      kTestSource1,
      [](const std::string& source_id, const SettingsService& settings) {
        return nullptr;
      });
  ConfigureTrustedSource(kTestSource1);
  MockLockedSettingsContainer* container =
      MakeContainer(MakeDocument(kTestSource1));
  container->GetVersionComponent(kTestSource1)->SetSourceId(kTestSource1);
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusValidationError,
            InsertBlob(kTestSource1, parser_.Register(container), {}));
}

TEST_F(SettingsDocumentManagerTest, InsertBlobValidationErrorSourceFailure) {
  ConfigureTrustedSource(kTestSource1);
  MockLockedSettingsContainer* container =
      MakeContainer(MakeDocument(kTestSource1));
  container->set_valid(false);
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusValidationError,
            InsertBlob(kTestSource1, parser_.Register(container), {}));
}

TEST_F(SettingsDocumentManagerTest,
       InsertBlobValidationErrorVersionStampFailure) {
  ConfigureTrustedSource(kTestSource1);
  MockLockedSettingsContainer* container =
      MakeContainer(MakeDocument(kTestSource1));
  container->GetVersionComponent(kTestSource1)->SetSourceId(kTestSource1);
  container->GetVersionComponent(kTestSource1)->set_valid(false);
  EXPECT_EQ(SettingsDocumentManager::kInsertionStatusValidationError,
            InsertBlob(kTestSource1, parser_.Register(container), {}));
}

TEST_F(SettingsDocumentManagerTest, InsertBlobValidationErrorBadPayload) {
  ConfigureTrustedSource(kTestSource1);
  EXPECT_EQ(
      SettingsDocumentManager::kInsertionStatusBadPayload,
      InsertBlob(kTestSource1, parser_.Register(MakeContainer(nullptr)), {}));
}

TEST_F(SettingsDocumentManagerTest, InsertBlobOnStartUp) {
  AddSentinelValue(kTestSource0);
  ReinitializeSettingsDocumentManager();
  CheckSentinelValues({kTestSource0}, {});
}

TEST_F(SettingsDocumentManagerTest, ContainerNotParseableOnRevalidation) {
  ConfigureTrustedSource(kTestSource1);
  MockLockedSettingsContainer* container = AddSentinelValue(kTestSource1);
  parser_.Unregister(container);
  SetTrustForSentinelKey(kTestSource1, kSettingStatusWithdrawn);
  CheckSentinelValues({}, {kTestSource1});
}

TEST_F(SettingsDocumentManagerTest, ContainerValidationFailureOnRevalidation) {
  ConfigureTrustedSource(kTestSource1);
  MockLockedSettingsContainer* container = AddSentinelValue(kTestSource1);
  container->set_valid(false);
  SetTrustForSentinelKey(kTestSource1, kSettingStatusWithdrawn);
  CheckSentinelValues({}, {kTestSource1});
}

TEST_F(SettingsDocumentManagerTest,
    VersionComponentValidationFailureOnRevalidation) {
  ConfigureTrustedSource(kTestSource1);
  MockLockedSettingsContainer* container = AddSentinelValue(kTestSource1);
  container->GetVersionComponent(kTestSource1)->set_valid(false);
  SetTrustForSentinelKey(kTestSource1, kSettingStatusWithdrawn);
  CheckSentinelValues({}, {kTestSource1});
}

}  // namespace fides
