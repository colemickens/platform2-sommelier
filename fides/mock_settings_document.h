// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIDES_MOCK_SETTINGS_DOCUMENT_H_
#define FIDES_MOCK_SETTINGS_DOCUMENT_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "fides/settings_document.h"
#include "fides/version_stamp.h"

namespace fides {

class MockSettingsDocument : public SettingsDocument {
 public:
  explicit MockSettingsDocument(const VersionStamp& version_stamp);
  ~MockSettingsDocument() override;

  // Returns a copy of the current document.
  std::unique_ptr<MockSettingsDocument> Clone() const;

  // SettingsDocument:
  BlobRef GetValue(const Key& key) const override;
  std::set<Key> GetKeys(const Key& prefix) const override;
  std::set<Key> GetDeletions(const Key& prefix) const override;
  VersionStamp GetVersionStamp() const override;
  bool HasKeysOrDeletions(const Key& prefix) const override;

  void SetKey(const Key& key, const std::string& value);
  void ClearKey(const Key& key);
  void ClearKeys();

  void SetDeletion(const Key& key);
  void ClearDeletion(const Key& key);
  void ClearDeletions();

 private:
  const VersionStamp version_stamp_;
  std::map<Key, std::string> key_value_map_;
  std::set<Key> deletions_;

  DISALLOW_COPY_AND_ASSIGN(MockSettingsDocument);
};

}  // namespace fides

#endif  // FIDES_MOCK_SETTINGS_DOCUMENT_H_
