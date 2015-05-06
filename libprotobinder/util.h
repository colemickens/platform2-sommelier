// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_UTIL_H_
#define LIBPROTOBINDER_UTIL_H_

#include <map>

namespace protobinder {
namespace internal {

// Erases all entries matching both |key| and |value| from |map|.
// Returns the number of erased entries.
template<typename K, typename V>
size_t EraseMultimapEntries(std::multimap<K, V>* map,
                            const K& key,
                            const V& value) {
  size_t num_erased = 0;
  auto range = map->equal_range(key);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second == value) {
      map->erase(it);
      num_erased++;
    }
  }
  return num_erased;
}

}  // namespace internal
}  // namespace protobinder

#endif  // LIBPROTOBINDER_UTIL_H_
