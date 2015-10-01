// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIDES_SOURCE_H_
#define FIDES_SOURCE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <base/macros.h>

#include "fides/key.h"
#include "fides/source_delegate.h"

namespace fides {

class SettingsDocument;
class SettingsService;

// Setting status values, in most permissive to least permissive order.
enum SettingStatus {
  // Settings from the are considered valid and setting updates are accepted.
  kSettingStatusActive,
  // Any settings provided by the source are already present in the system
  // remain valid. However, no new settings are accepted.
  kSettingStatusWithdrawn,
  // All settings are considered invalid.
  kSettingStatusInvalid,
};

// Decodes a string to the corresponding SettingStatus enum value. If the
// provided string doesn't match one of the valid status names,
// kSettingStatusInvalid will be returned.
SettingStatus SettingStatusFromString(const std::string& status_string);

// Returns the string identifier for the provided setting status.
std::string SettingStatusToString(SettingStatus status);

// Makes a Key for the prefix all configuration for the source identified by
// |source_id| is residing in.
Key MakeSourceKey(const std::string& source_id);

// A high-level description of a configuration source. This defines the
// interface that is used to perform validity checks of a settings documents
// against sources configured in the system.
class Source {
 public:
  explicit Source(const std::string& id);
  ~Source();

  const std::string& id() const { return id_; }
  const std::string& name() const { return name_; }
  SettingStatus status() const { return status_; }
  const SourceDelegate* delegate() const { return delegate_.get(); }
  const std::vector<std::string>& blob_formats() const { return blob_formats_; }

  // Checks whether this source has access control rules within |threshold| for
  // all keys touched by |document|. This checks that the relevant (i.e. most
  // specific) access control rule for each key is at least as permissive as
  // |threshold|.
  bool CheckAccess(const SettingsDocument* document,
                   SettingStatus threshold) const;

  // Updates the source from settings.
  //
  // TODO(mnissler): Consider returning information on what changed such that
  // callers don't need to reprocess the entire source definition.
  bool Update(const SourceDelegateFactoryFunction& delegate_factory_function,
              const SettingsService& settings);

 private:
  using AccessRuleMap = std::map<Key, SettingStatus>;

  // Finds the most specific matching access rule for |key|. Returns
  // |access_.end()| if no rule matches.
  AccessRuleMap::const_iterator FindMatchingAccessRule(const Key& key) const;

  // The source id.
  const std::string id_;

  // Friendly name for the source.
  std::string name_;

  // The current status of this source.
  SettingStatus status_ = kSettingStatusInvalid;

  // The delegate.
  std::unique_ptr<SourceDelegate> delegate_;

  // Access control rules. This maps key prefixes to SettingStatus values
  // indicating whether the source may provide values for keys that match the
  // prefix. When there are multiple matching prefixes for a key, the rule
  // corresponding to the longest prefix wins. If there is no matching access
  // control rule, the default to use is kSettingStatusInvalid.
  AccessRuleMap access_;

  // The set of blob formats allowed for parsing blobs belonging to this source.
  // These formats are tried in order.
  std::vector<std::string> blob_formats_;

  DISALLOW_COPY_AND_ASSIGN(Source);
};

}  // namespace fides

#endif  // FIDES_SOURCE_H_
