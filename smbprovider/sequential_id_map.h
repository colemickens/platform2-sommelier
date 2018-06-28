// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_SEQUENTIAL_ID_MAP_H_
#define SMBPROVIDER_SEQUENTIAL_ID_MAP_H_

#include <base/macros.h>

namespace smbprovider {

// Class that maps an increasing int32_t ID to another type. Used for
// handing out pseudo file descriptors.
//
// TODO(zentaro): WIP for crbug/857487
//   - Add map member variable
//   - Add method to insert value
//   - Add method to query value
//   - Add method to remove value
template <typename T>
class SequentialIdMap {
 public:
  SequentialIdMap() = default;
  ~SequentialIdMap() = default;

  DISALLOW_COPY_AND_ASSIGN(SequentialIdMap);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_SEQUENTIAL_ID_MAP_H_
