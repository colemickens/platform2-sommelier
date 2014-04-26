// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_TESTING_H_
#define SHILL_TESTING_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace shill {

// A Google Mock action (similar to testing::ReturnPointee) that takes a pointer
// to a scoped_ptr object, releases and returns the raw pointer managed by the
// scoped_ptr object when the action is invoked.
//
// Example usage:
//
//   TEST(FactoryTest, CreateStuff) {
//     MockFactory factory;
//     scoped_ptr<Stuff> stuff(new Stuff());
//     EXPECT_CALL(factory, CreateStuff())
//         .WillOnce(ReturnAndReleasePointee(&stuff));
//   }
//
// If |factory.CreateStuff()| is called, the ownership of the Stuff object
// managed by |stuff| is transferred to the caller of |factory.CreateStuff()|.
// Otherwise, the Stuff object will be destroyed once |stuff| goes out of
// scope when the test completes.
ACTION_P(ReturnAndReleasePointee, scoped_pointer) {
  return scoped_pointer->release();
}

MATCHER(IsSuccess, "") {
  return arg.IsSuccess();
}

MATCHER(IsFailure, "") {
  return arg.IsFailure();
}

MATCHER_P2(ErrorIs, error_type, error_message, "") {
  return error_type == arg.type() && error_message == arg.message();
}

MATCHER(IsNullRefPtr, "") {
  return !arg.get();
}

MATCHER(NotNullRefPtr, "") {
  return arg.get();
}

}  // namespace shill

#endif  // SHILL_TESTING_H_
