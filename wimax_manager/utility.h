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
  typename std::map<KeyType, ValueType>::const_iterator map_iterator;
  for (map_iterator = m.begin(); map_iterator != m.end(); ++map_iterator) {
    keys.insert(map_iterator->first);
  }
  return keys;
}

template <typename KeyType, typename ValueType>
void RemoveKeysFromMap(std::map<KeyType, ValueType> *m,
                       const std::set<KeyType> &keys_to_remove) {
  CHECK(m);

  typename std::set<KeyType>::const_iterator key_iterator;
  for (key_iterator = keys_to_remove.begin();
       key_iterator != keys_to_remove.end(); ++key_iterator) {
    m->erase(*key_iterator);
  }
}

}  // wimax_manager

#endif  // WIMAX_MANAGER_UTILITY_H_
