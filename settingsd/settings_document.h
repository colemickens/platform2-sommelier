// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_SETTINGS_DOCUMENT_H_
#define SETTINGSD_SETTINGS_DOCUMENT_H_

#include <base/macros.h>
#include <base/values.h>
#include <set>
#include <string>

#include "settingsd/key.h"
#include "settingsd/version_stamp.h"

namespace settingsd {

// Interface for a collection of settings residing in the same serialized
// container.
class SettingsDocument {
 public:
  virtual ~SettingsDocument() {}

  // Retrieves the value for the setting identified by |key|. If this settings
  // document does not contain a setting with that key, the method returns a
  // nullptr.
  virtual const base::Value* GetValue(const Key& key) const = 0;

  // Returns a list of all keys that have value assignments and are equal to or
  // have |prefix| as an ancestor.
  virtual std::set<Key> GetKeys(const Key& prefix) const = 0;

  // Returns a list of all keys whose subtrees are being deleted by this
  // document and that are either equal to or have |prefix| as an ancestor.
  virtual std::set<Key> GetDeletions(const Key& prefix) const = 0;

  // Returns the version stamp for this settings document.
  virtual const VersionStamp& GetVersionStamp() const = 0;

  // Returns true if the document modifies keys that are equal to or have
  // |prefix| as an ancestor. Otherwise, returns false. Modifications here could
  // be value assignments for |prefix| or deletions of subtrees containing
  // |prefix|.
  virtual bool HasKeysOrDeletions(const Key& prefix) const = 0;

  // Returns true if any of the keys or subtree deletions in document |A| and
  // |B| overlap, i.e. there are keys that are equal or one is the ancestor of
  // the other.
  static bool HasOverlap(const SettingsDocument& A, const SettingsDocument& B);

 private:
  DISALLOW_ASSIGN(SettingsDocument);
};

}  // namespace settingsd

#endif  // SETTINGSD_SETTINGS_DOCUMENT_H_
