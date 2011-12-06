// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OBJECT_MOCK_H
#define CHAPS_OBJECT_MOCK_H

#include "chaps/object.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chaps/attributes.h"
#include "pkcs11/cryptoki.h"

namespace chaps {

class ObjectMock : public Object {
 public:
  ObjectMock() {}
  MOCK_CONST_METHOD0(GetSize, int ());
  MOCK_METHOD2(GetAttributes, CK_RV (CK_ATTRIBUTE_PTR, int));
  MOCK_METHOD2(SetAttributes, CK_RV (const CK_ATTRIBUTE_PTR, int));

 private:
  DISALLOW_COPY_AND_ASSIGN(ObjectMock);
};

}  // namespace chaps

#endif  // CHAPS_OBJECT_MOCK_H
