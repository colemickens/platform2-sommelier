// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_CHAPS_FACTORY_MOCK_H
#define CHAPS_CHAPS_FACTORY_MOCK_H

#include "chaps/chaps_factory.h"

namespace chaps {

class ChapsFactoryMock : public ChapsFactory {
 public:
  MOCK_METHOD4(CreateSession, Session*(int, ObjectPool*, TPMUtility*, bool));
  MOCK_METHOD1(CreatePersistentObjectPool, ObjectPool*(const std::string&));
};

}  // namespace chaps

#endif  // CHAPS_CHAPS_FACTORY_MOCK_H
