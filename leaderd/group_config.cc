// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leaderd/group_config.h"

#include <base/rand_util.h>

#include "leaderd/errors.h"

namespace leaderd {
namespace group_options {

const char kMinWandererTimeoutSeconds[] = "min_wanderer_timeout_ms";
const char kMaxWandererTimeoutSeconds[] = "max_wanderer_timeout_ms";
const char kLeaderSteadyStateTimeoutSeconds[] =
    "leader_steady_state_timeout_ms";
const char kIsPersistent[] = "persistent";

}  // namespace group_options

namespace {

template<typename T>
bool GetValue(const chromeos::Any& any, T* value) {
  if (!any.IsTypeCompatible<T>()) {
    return false;
  }
  *value = any.Get<T>();
  return true;
}

}  // namespace


bool GroupConfig::Load(const std::map<std::string, chromeos::Any>& options,
                       chromeos::ErrorPtr* error) {
  size_t parsed_fields = 0;
  auto it = options.find(group_options::kMinWandererTimeoutSeconds);
  int32_t timeout_value;  // This decides the parsed type of our DBus interface.
  if (it != options.end()) {
    if (!GetValue(it->second, &timeout_value) ||
        timeout_value < 0) {
      chromeos::Error::AddTo(error, FROM_HERE, errors::kDomain,
                             errors::kBadOptions, "Bad min wanderer time");
      return false;
    }
    min_wanderer_timeout_ms_ = timeout_value;
    ++parsed_fields;
  }
  it = options.find(group_options::kMaxWandererTimeoutSeconds);
  if (it != options.end()) {
    if (!GetValue(it->second, &timeout_value) ||
        timeout_value < 0) {
      chromeos::Error::AddTo(error, FROM_HERE, errors::kDomain,
                             errors::kBadOptions, "Bad max wanderer time");
      return false;
    }
    max_wanderer_timeout_ms_ = timeout_value;
    ++parsed_fields;
  }
  if (min_wanderer_timeout_ms_ > max_wanderer_timeout_ms_) {
    chromeos::Error::AddTo(error, FROM_HERE, errors::kDomain,
                           errors::kBadOptions,
                           "Min wanderer timeout greater than max");
    return false;
  }
  it = options.find(group_options::kLeaderSteadyStateTimeoutSeconds);
  if (it != options.end()) {
    if (!GetValue(it->second, &timeout_value) ||
        timeout_value < 0) {
      chromeos::Error::AddTo(error, FROM_HERE, errors::kDomain,
                             errors::kBadOptions,
                             "Bad leader steady state timeout");
      return false;
    }
    leader_steady_state_timeout_ms_ = timeout_value;
    ++parsed_fields;
  }
  it = options.find(group_options::kIsPersistent);
  if (it != options.end()) {
    if (!GetValue(it->second, &is_persistent_)) {
      chromeos::Error::AddTo(error, FROM_HERE, errors::kDomain,
                             errors::kBadOptions, "Bad persistent value.");
      return false;
    }
    ++parsed_fields;
  }
  if (parsed_fields != options.size()) {
    chromeos::Error::AddTo(error, FROM_HERE, errors::kDomain,
                           errors::kBadOptions, "Got unexpected options.");
    return false;
  }
  return true;
}

uint64_t GroupConfig::PickWandererTimeoutMs() const {
  const uint64_t offset_range = 1 + (max_wanderer_timeout_ms_ -
                                     min_wanderer_timeout_ms_);
  return min_wanderer_timeout_ms_ + base::RandGenerator(offset_range);
}

}  // namespace leaderd
