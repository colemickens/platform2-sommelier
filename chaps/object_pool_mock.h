// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OBJECT_POOL_MOCK_H
#define CHAPS_OBJECT_POOL_MOCK_H

namespace chaps {

class ObjectPoolMock : public ObjectPool {
 public:
  MOCK_METHOD2(GetInternalBlob, bool (int, std::string*));
  MOCK_METHOD2(SetInternalBlob, bool (int, const std::string&));
  MOCK_METHOD1(SetKey, void (const std::string&));
};

}  // namespace

#endif  // CHAPS_OBJECT_POOL_MOCK_H
