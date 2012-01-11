// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_CALLBACK_
#define SHILL_MOCK_CALLBACK_

#include <base/basictypes.h>
#include <base/callback_old.h>
#include <gmock/gmock.h>

namespace shill {

class EmptyClass {};

class MockCallback
    : public CallbackWithReturnValueImpl<
  EmptyClass, bool (EmptyClass::*)(), bool> {
 public:
  MockCallback();
  virtual ~MockCallback();

  MOCK_METHOD0(Run, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCallback);
};

}  // namespace shill

#endif  // SHILL_MOCK_CALLBACK_
