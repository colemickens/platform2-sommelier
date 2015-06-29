// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/source.h"

#include <base/logging.h>
#include <base/values.h>

#include "settingsd/settings_document.h"
#include "settingsd/settings_keys.h"
#include "settingsd/settings_service.h"

namespace settingsd {

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
  return Key(keys::kSettingsdPrefix).Extend({keys::kSources, source_id});
}

Source::Source(const std::string& id)
    : id_(id), delegate_(new DummySourceDelegate()) {}

bool Source::CheckAccess(const SettingsDocument* document,
                         SettingStatus threshold) const {
  if (status_ > threshold)
    return false;

  const Key trust_config_area_begin(
      Key(keys::kSettingsdPrefix).Extend({keys::kSources}));
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

void Source::Update(
    const SourceDelegateFactoryFunction& delegate_factory_function,
    const SettingsService& settings) {
  const base::Value* value;

  name_.clear();
  value = settings.GetValue(MakeSourceKey(id_).Extend({keys::sources::kName}));
  if (value)
    value->GetAsString(&name_);

  value =
      settings.GetValue(MakeSourceKey(id_).Extend({keys::sources::kStatus}));
  std::string status_string;
  if (value)
    value->GetAsString(&status_string);
  status_ = SettingStatusFromString(status_string);

  delegate_ = delegate_factory_function(id_, settings);

  access_.clear();
  const Key access_key_prefix(
      MakeSourceKey(id_).Extend({keys::sources::kAccess}));
  const std::set<Key> access_keys(settings.GetKeys(access_key_prefix));
  for (Key access_key : access_keys) {
    status_string.clear();
    value = settings.GetValue(access_key);
    if (value)
      value->GetAsString(&status_string);
    Key suffix;
    if (access_key.Suffix(access_key_prefix, &suffix))
      access_[suffix] = SettingStatusFromString(status_string);
    else
      NOTREACHED() << "Invalid access key " << access_key.ToString();
  }
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

}  // namespace settingsd
