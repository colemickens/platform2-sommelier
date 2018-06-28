// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_SEQUENTIAL_ID_MAP_H_
#define SMBPROVIDER_SEQUENTIAL_ID_MAP_H_

#include <map>
#include <utility>

#include <base/logging.h>
#include <base/macros.h>

namespace smbprovider {

// Class that maps an increasing int32_t ID to another type. Used for
// handing out pseudo file descriptors.
//
// TODO(zentaro): WIP for crbug/857487
//   - Add method to remove value
template <typename T>
class SequentialIdMap {
 public:
  SequentialIdMap() = default;
  ~SequentialIdMap() = default;

  int32_t Insert(T value) {
    DCHECK_EQ(0, ids_.count(next_id_));

    ids_.insert({next_id_, std::move(value)});
    return next_id_++;
  }

  typename std::map<int32_t, T>::const_iterator Find(int32_t id) const {
    return ids_.find(id);
  }

  bool Contains(int32_t id) const { return ids_.count(id) > 0; }

  typename std::map<int32_t, T>::const_iterator End() const {
    return ids_.end();
  }

 private:
  std::map<int32_t, T> ids_;
  int32_t next_id_ = 0;
  DISALLOW_COPY_AND_ASSIGN(SequentialIdMap);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_SEQUENTIAL_ID_MAP_H_
