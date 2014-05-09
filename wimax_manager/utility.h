// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_UTILITY_H_
#define WIMAX_MANAGER_UTILITY_H_

#include <set>
#include <map>

#include <base/logging.h>

namespace wimax_manager {

template <typename KeyType, typename ValueType>
std::set<KeyType> GetKeysOfMap(const std::map<KeyType, ValueType> &m) {
  std::set<KeyType> keys;
  for (const auto &key_value : m) {
    keys.insert(key_value.first);
  }
  return keys;
}

template <typename KeyType, typename ValueType>
void RemoveKeysFromMap(std::map<KeyType, ValueType> *m,
                       const std::set<KeyType> &keys_to_remove) {
  CHECK(m);

  for (const auto &key : keys_to_remove) {
    m->erase(key);
  }
}

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_UTILITY_H_
