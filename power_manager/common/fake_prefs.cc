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
  for (PrefsObserver& observer : observers_)
    observer.OnPrefChanged(name);
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

bool FakePrefs::GetInt64(const std::string& name, int64_t* value) {
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
  int64_t int_value = 0;
  if (!GetInt64(name, &int_value))
    return false;
  *value = int_value != 0;
  return true;
}

void FakePrefs::SetString(const std::string& name, const std::string& value) {
  Unset(name);
  string_prefs_[name] = value;
}

void FakePrefs::SetInt64(const std::string& name, int64_t value) {
  Unset(name);
  int64_prefs_[name] = value;
}

void FakePrefs::SetDouble(const std::string& name, double value) {
  Unset(name);
  double_prefs_[name] = value;
}

}  // namespace power_manager
