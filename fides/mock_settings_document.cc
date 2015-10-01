// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/mock_settings_document.h"

#include "fides/identifier_utils.h"

namespace fides {

MockSettingsDocument::MockSettingsDocument(const VersionStamp& version_stamp)
    : version_stamp_(version_stamp) {}

MockSettingsDocument::~MockSettingsDocument() {}

std::unique_ptr<MockSettingsDocument> MockSettingsDocument::Clone() const {
  std::unique_ptr<MockSettingsDocument> copy(
      new MockSettingsDocument(version_stamp_));
  copy->deletions_ = deletions_;
  copy->key_value_map_ = key_value_map_;
  return copy;
}

BlobRef MockSettingsDocument::GetValue(const Key& key) const {
  auto entry = key_value_map_.find(key);
  return entry != key_value_map_.end() ? BlobRef(&entry->second) : BlobRef();
}

std::set<Key> MockSettingsDocument::GetKeys(const Key& prefix) const {
  std::set<Key> result;
  for (const auto& entry : utils::GetRange(prefix, key_value_map_))
    result.insert(entry.first);
  return result;
}

std::set<Key> MockSettingsDocument::GetDeletions(const Key& prefix) const {
  std::set<Key> result;
  for (const auto& entry : utils::GetRange(prefix, deletions_))
    result.insert(entry);
  return result;
}

VersionStamp MockSettingsDocument::GetVersionStamp() const {
  return version_stamp_;
}

bool MockSettingsDocument::HasKeysOrDeletions(const Key& prefix) const {
  return utils::HasKeys(prefix, key_value_map_) ||
         utils::HasKeys(prefix, deletions_);
}

void MockSettingsDocument::SetKey(const Key& key, const std::string& value) {
  key_value_map_.insert(std::make_pair(key, std::move(value)));
}

void MockSettingsDocument::ClearKey(const Key& key) {
  key_value_map_.erase(key);
}

void MockSettingsDocument::ClearKeys() {
  key_value_map_.clear();
}

void MockSettingsDocument::SetDeletion(const Key& key) {
  deletions_.insert(key);
}

void MockSettingsDocument::ClearDeletion(const Key& key) {
  deletions_.erase(key);
}

void MockSettingsDocument::ClearDeletions() {
  deletions_.clear();
}

}  // namespace fides
