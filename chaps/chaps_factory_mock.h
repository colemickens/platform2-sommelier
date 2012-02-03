// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_CHAPS_FACTORY_MOCK_H
#define CHAPS_CHAPS_FACTORY_MOCK_H

#include "chaps/chaps_factory.h"

#include <base/basictypes.h>
#include <gmock/gmock.h>

namespace chaps {

class ChapsFactoryMock : public ChapsFactory {
 public:
  ChapsFactoryMock();
  virtual ~ChapsFactoryMock();

  MOCK_METHOD5(CreateSession, Session*(int,
                                       ObjectPool*,
                                       TPMUtility*,
                                       HandleGenerator*,
                                       bool));
  MOCK_METHOD2(CreateObjectPool, ObjectPool*(HandleGenerator*, ObjectStore*));
  MOCK_METHOD1(CreateObjectStore, ObjectStore*(const FilePath&));
  MOCK_METHOD0(CreateObject, Object*());
  MOCK_METHOD1(CreateObjectPolicy, ObjectPolicy*(CK_OBJECT_CLASS));

 private:
  DISALLOW_COPY_AND_ASSIGN(ChapsFactoryMock);
};

}  // namespace chaps

#endif  // CHAPS_CHAPS_FACTORY_MOCK_H
