// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/policy/registry_dict.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/values.h"

namespace policy {

bool CaseInsensitiveStringCompare::operator()(const std::string& a,
                                              const std::string& b) const {
  return base::CompareCaseInsensitiveASCII(a, b) < 0;
}

RegistryDict::RegistryDict() {}

RegistryDict::~RegistryDict() {
  ClearKeys();
  ClearValues();
}

RegistryDict* RegistryDict::GetKey(const std::string& name) {
  KeyMap::iterator entry = keys_.find(name);
  return entry != keys_.end() ? entry->second : NULL;
}

const RegistryDict* RegistryDict::GetKey(const std::string& name) const {
  KeyMap::const_iterator entry = keys_.find(name);
  return entry != keys_.end() ? entry->second : NULL;
}

void RegistryDict::SetKey(const std::string& name,
                          std::unique_ptr<RegistryDict> dict) {
  if (!dict) {
    RemoveKey(name);
    return;
  }

  RegistryDict*& entry = keys_[name];
  delete entry;
  entry = dict.release();
}

std::unique_ptr<RegistryDict> RegistryDict::RemoveKey(const std::string& name) {
  std::unique_ptr<RegistryDict> result;
  KeyMap::iterator entry = keys_.find(name);
  if (entry != keys_.end()) {
    result.reset(entry->second);
    keys_.erase(entry);
  }
  return result;
}

void RegistryDict::ClearKeys() {
  STLDeleteValues(&keys_);
}

base::Value* RegistryDict::GetValue(const std::string& name) {
  ValueMap::iterator entry = values_.find(name);
  return entry != values_.end() ? entry->second : NULL;
}

const base::Value* RegistryDict::GetValue(const std::string& name) const {
  ValueMap::const_iterator entry = values_.find(name);
  return entry != values_.end() ? entry->second : NULL;
}

void RegistryDict::SetValue(const std::string& name,
                            std::unique_ptr<base::Value> dict) {
  if (!dict) {
    RemoveValue(name);
    return;
  }

  base::Value*& entry = values_[name];
  delete entry;
  entry = dict.release();
}

std::unique_ptr<base::Value> RegistryDict::RemoveValue(
    const std::string& name) {
  std::unique_ptr<base::Value> result;
  ValueMap::iterator entry = values_.find(name);
  if (entry != values_.end()) {
    result.reset(entry->second);
    values_.erase(entry);
  }
  return result;
}

void RegistryDict::ClearValues() {
  STLDeleteValues(&values_);
}

void RegistryDict::Merge(const RegistryDict& other) {
  for (KeyMap::const_iterator entry(other.keys_.begin());
       entry != other.keys_.end(); ++entry) {
    RegistryDict*& subdict = keys_[entry->first];
    if (!subdict)
      subdict = new RegistryDict();
    subdict->Merge(*entry->second);
  }

  for (ValueMap::const_iterator entry(other.values_.begin());
       entry != other.values_.end(); ++entry) {
    SetValue(entry->first, entry->second->CreateDeepCopy());
  }
}

void RegistryDict::Swap(RegistryDict* other) {
  keys_.swap(other->keys_);
  values_.swap(other->values_);
}

}  // namespace policy
