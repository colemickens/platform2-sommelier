// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_MOCK_SETTINGS_DOCUMENT_H_
#define SETTINGSD_MOCK_SETTINGS_DOCUMENT_H_

#include <base/values.h>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "settingsd/settings_document.h"
#include "settingsd/version_stamp.h"

namespace settingsd {

class MockSettingsDocument : public SettingsDocument {
 public:
  explicit MockSettingsDocument(const VersionStamp& version_stamp);
  ~MockSettingsDocument() override;

  // Returns a copy of the current document.
  std::unique_ptr<MockSettingsDocument> Clone() const;

  // SettingsDocument:
  const base::Value* GetValue(const Key& key) const override;
  std::set<Key> GetKeys(const Key& prefix) const override;
  std::set<Key> GetDeletions(const Key& prefix) const override;
  const VersionStamp& GetVersionStamp() const override;
  bool HasKeysOrDeletions(const Key& prefix) const override;

  void SetKey(const Key& key, std::unique_ptr<base::Value> value);
  void ClearKey(const Key& key);
  void ClearKeys();

  void SetDeletion(const Key& key);
  void ClearDeletion(const Key& key);
  void ClearDeletions();

 private:
  const VersionStamp version_stamp_;
  std::map<Key, std::unique_ptr<base::Value>> key_value_map_;
  std::set<Key> deletions_;

  DISALLOW_COPY_AND_ASSIGN(MockSettingsDocument);
};

}  // namespace settingsd

#endif  // SETTINGSD_MOCK_SETTINGS_DOCUMENT_H_
