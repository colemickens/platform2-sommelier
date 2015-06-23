// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/mock_settings_document.h"

#include "settingsd/identifier_utils.h"

namespace settingsd {

MockSettingsDocument::MockSettingsDocument(const VersionStamp& version_stamp)
    : version_stamp_(version_stamp) {}

MockSettingsDocument::~MockSettingsDocument() {}

const base::Value* MockSettingsDocument::GetValue(const Key& key) const {
  auto entry = key_value_map_.find(key);
  return entry != key_value_map_.end() ? entry->second.get() : nullptr;
}

std::set<Key> MockSettingsDocument::GetKeys(const Key& key) const {
  std::set<Key> result;
  for (const auto& entry : utils::GetRange(key, key_value_map_))
    result.insert(entry.first);
  return result;
}

std::set<Key> MockSettingsDocument::GetDeletions(const Key& key) const {
  std::set<Key> result;
  for (const auto& entry : utils::GetRange(key, deletions_))
    result.insert(entry);
  return result;
}

const VersionStamp& MockSettingsDocument::GetVersionStamp() const {
  return version_stamp_;
}

void MockSettingsDocument::SetEntry(const Key& key,
                                    std::unique_ptr<base::Value> value) {
  key_value_map_.insert(std::make_pair(key, std::move(value)));
}

void MockSettingsDocument::SetDeletion(const Key& key) {
  deletions_.insert(key);
}

}  // namespace settingsd
