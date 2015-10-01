// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/source.h"

#include <base/logging.h>
#include <base/strings/string_split.h>
#include <string>

#include "fides/settings_document.h"
#include "fides/settings_keys.h"
#include "fides/settings_service.h"

namespace fides {

namespace {

// Setting status strings.
const char* const kSettingStatusStrings[] = {"active", "withdrawn", "invalid"};

}  // namespace

SettingStatus SettingStatusFromString(const std::string& status_string) {
  for (SettingStatus status :
       {kSettingStatusActive, kSettingStatusWithdrawn, kSettingStatusInvalid}) {
    CHECK_LT(status, sizeof(kSettingStatusStrings));
    if (status_string == kSettingStatusStrings[status])
      return status;
  }

  return kSettingStatusInvalid;
}

std::string SettingStatusToString(SettingStatus status) {
  CHECK_LT(status, sizeof(kSettingStatusStrings));
  return kSettingStatusStrings[status];
}

Key MakeSourceKey(const std::string& source_id) {
  // TODO(mnissler): Handle nested sources properly.
  return Key(keys::kFidesPrefix).Extend({keys::kSources, source_id});
}

Source::Source(const std::string& id)
    : id_(id), delegate_(new DummySourceDelegate()) {}

Source::~Source() {}

bool Source::CheckAccess(const SettingsDocument* document,
                         SettingStatus threshold) const {
  if (status_ > threshold)
    return false;

  const Key trust_config_area_begin(
      Key(keys::kFidesPrefix).Extend({keys::kSources}));
  const Key trust_config_area_end(MakeSourceKey(id_).PrefixUpperBound());

  for (auto& key : document->GetKeys(Key())) {
    if (trust_config_area_begin <= key && key < trust_config_area_end)
      return false;

    auto access_rule = FindMatchingAccessRule(key);
    if (access_rule == access_.end() || access_rule->second > threshold)
      return false;
  }

  for (auto& deletion : document->GetDeletions(Key())) {
    if ((trust_config_area_begin <= deletion &&
         deletion < trust_config_area_end) ||
        deletion.IsPrefixOf(trust_config_area_begin)) {
      return false;
    }

    auto access_rule = FindMatchingAccessRule(deletion);
    if (access_rule == access_.end() || access_rule->second > threshold)
      return false;

    // Check all access rules within the set of deleted keys.
    for (; access_rule != access_.end() &&
           deletion.IsPrefixOf(access_rule->first);
         ++access_rule) {
      if (access_rule->second > threshold)
        return false;
    }
  }

  return true;
}

bool Source::Update(
    const SourceDelegateFactoryFunction& delegate_factory_function,
    const SettingsService& settings) {
  bool has_config = false;

  name_.clear();
  BlobRef name_value =
      settings.GetValue(MakeSourceKey(id_).Extend({keys::sources::kName}));
  if (name_value.valid()) {
    has_config = true;
    name_ = name_value.ToString();
  }

  BlobRef status_value =
      settings.GetValue(MakeSourceKey(id_).Extend({keys::sources::kStatus}));
  std::string status_string;
  if (status_value.valid()) {
    has_config = true;
    status_string = status_value.ToString();
  }
  status_ = SettingStatusFromString(status_string);

  delegate_ = delegate_factory_function(id_, settings);
  has_config |= !!delegate_;
  if (!delegate_)
    delegate_.reset(new DummySourceDelegate());

  access_.clear();
  const Key access_key_prefix(
      MakeSourceKey(id_).Extend({keys::sources::kAccess}));
  const std::set<Key> access_keys(settings.GetKeys(access_key_prefix));
  for (Key access_key : access_keys) {
    has_config = true;
    status_string.clear();
    BlobRef value = settings.GetValue(access_key);
    if (value.valid())
      status_string = value.ToString();
    Key suffix;
    if (access_key.Suffix(access_key_prefix, &suffix))
      access_[suffix] = SettingStatusFromString(status_string);
    else
      NOTREACHED() << "Invalid access key " << access_key.ToString();
  }

  blob_formats_.clear();
  BlobRef formats_value = settings.GetValue(
      MakeSourceKey(id_).Extend({keys::sources::kBlobFormat}));
  if (formats_value.valid()) {
    has_config = true;
    blob_formats_ =
        base::SplitString(formats_value.ToString(), ",", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
  }

  return has_config;
}

Source::AccessRuleMap::const_iterator Source::FindMatchingAccessRule(
    const Key& key) const {
  Key lookup_key = key;
  auto rule = access_.upper_bound(lookup_key);
  while (rule != access_.begin()) {
    --rule;
    if (rule->first.IsPrefixOf(key))
      return rule;
    lookup_key = lookup_key.CommonPrefix(rule->first);
    rule = access_.upper_bound(lookup_key);
  }

  return access_.end();
}

}  // namespace fides
