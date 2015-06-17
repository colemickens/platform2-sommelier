// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/mock_settings_document.h"

#include "settingsd/identifier_utils.h"

namespace settingsd {

MockSettingsDocument::MockSettingsDocument(const VersionStamp& version_stamp)
    : version_stamp_(version_stamp) {
}

MockSettingsDocument::~MockSettingsDocument() {
}

const base::Value* MockSettingsDocument::GetValue(
    const std::string& prefix) const {
  auto entry = prefix_value_map_.find(prefix);
  if (entry != prefix_value_map_.end())
    return entry->second.get();
  return nullptr;
}

std::set<std::string> MockSettingsDocument::GetActiveChildPrefixes(
    const std::string& prefix) const {
  std::set<std::string> result;
  for (const auto& entry : utils::GetChildPrefixes(prefix, prefix_value_map_))
    result.insert(entry.first);
  return result;
}

const VersionStamp& MockSettingsDocument::GetVersionStamp() const {
  return version_stamp_;
}

void MockSettingsDocument::SetEntry(const std::string& prefix,
                                    std::unique_ptr<base::Value> value) {
  prefix_value_map_.insert(make_pair(prefix, std::move(value)));
}

}  // namespace settingsd
