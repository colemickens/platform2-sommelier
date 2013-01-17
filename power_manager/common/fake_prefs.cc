// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/fake_prefs.h"

#include "power_manager/common/prefs_observer.h"

namespace power_manager {

FakePrefs::FakePrefs() {}

FakePrefs::~FakePrefs() {}

void FakePrefs::Unset(const std::string& name) {
  int64_prefs_.erase(name);
  double_prefs_.erase(name);
  string_prefs_.erase(name);
}

void FakePrefs::NotifyObservers(const std::string& name) {
  FOR_EACH_OBSERVER(PrefsObserver, observers_, OnPrefChanged(name));
}

void FakePrefs::AddObserver(PrefsObserver* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void FakePrefs::RemoveObserver(PrefsObserver* observer) {
  CHECK(observer);
  observers_.RemoveObserver(observer);
}

bool FakePrefs::GetString(const std::string& name, std::string* value) {
  if (!string_prefs_.count(name))
    return false;
  *value = string_prefs_[name];
  return true;
}

bool FakePrefs::GetInt64(const std::string& name, int64* value) {
  if (!int64_prefs_.count(name))
    return false;
  *value = int64_prefs_[name];
  return true;
}

bool FakePrefs::GetDouble(const std::string& name, double* value) {
  if (!double_prefs_.count(name))
    return false;
  *value = double_prefs_[name];
  return true;
}

bool FakePrefs::GetBool(const std::string& name, bool* value) {
  int64 int_value = 0;
  if (!GetInt64(name, &int_value))
    return false;
  *value = int_value != 0;
  return true;
}

bool FakePrefs::SetString(const std::string& name, const std::string& value) {
  Unset(name);
  string_prefs_[name] = value;
  return true;
}

bool FakePrefs::SetInt64(const std::string& name, int64 value) {
  Unset(name);
  int64_prefs_[name] = value;
  return true;
}

bool FakePrefs::SetDouble(const std::string& name, double value) {
  Unset(name);
  double_prefs_[name] = value;
  return true;
}

}  // namespace power_manager
