// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_CALLBACK_
#define SHILL_MOCK_CALLBACK_

// This file contains a set of templates for MockCallback that correspond to the
// Callback templates in base/callback_old.h.  Any callback in
// base/callback_old.h can be replaced by one of the MockCallbacks of this file.
//
// For exmaple, to replace a Callback2 object created like this
//
//   void Object::DoStuff(int, string);
//   Callback2<int, string>::Type* callback =
//     NewCallback(obj, &Object::DoStuff);
//
// declare a MockCallback using the function signature of DoStuff, like this:
//
//   MockCallback<void(int, string)> *mock_callback = NewMockCallback();
//
// The template paramter is "void(int, string)", which indicates that the
// callback takes two parameters (an int and a string) and returns void.
//
// If you want to mock CallbackWithReturnValue<int>, do so like this:
//
//   MockCallback<int()> *mock_callback_with_return = NewMockCallback();
//
// (you can replace <int> with any type).
//
// You set expectations on the MockCallback like this:
//
//   EXPECT_CALL(*mock_callback, OnRun(42, "Beeblebrox"));
//   EXPECT_CALL(*mock_callback_with_return, OnRun()).WillOnce(Return(99));
//
// Note that the method on which the expectation is set is "OnRun" and not
// "Run".  This is because the Run method of the real callbacks is not declared
// as virtual (except in CallbackWithReturnValue).

#include <base/basictypes.h>
#include <base/callback_old.h>
#include <base/tuple.h>
#include <gmock/gmock.h>

namespace shill {

// The MockCallbacks are declared here.  This is the general template.
template <typename F>
class MockCallback;

// This is a specialized template for Callback0.
template <>
class MockCallback<void()> : public Callback0::Type {
 public:
  MOCK_METHOD0(OnRun, void());
  virtual void RunWithParams(const Tuple0 &) {
    OnRun();
  }
};

// This is a specialized template for CallbackWithReturnValue.
template <typename R>
class MockCallback<R()> : public CallbackWithReturnValue<R>::Type {
 public:
  MOCK_METHOD0_T(OnRun, R());
  virtual R  Run() {
    return OnRun();
  }
};

// This is a specialized template for Callback1.
template <typename Arg1>
class MockCallback<void(Arg1)> : public Callback1<Arg1>::Type {
 public:
  MOCK_METHOD1_T(OnRun, void(Arg1 arg1));
  virtual void RunWithParams(const Tuple1<Arg1> &params) {
    OnRun(params.a);
  }
};

// This is a specialized template for Callback2.
template <typename Arg1, typename Arg2>
class MockCallback<void(Arg1, Arg2)> : public Callback2<Arg1, Arg2>::Type {
 public:
  MOCK_METHOD2_T(OnRun, void(Arg1 arg1, Arg2 arg2));
  virtual void RunWithParams(const Tuple2<Arg1, Arg2> &params) {
    OnRun(params.a, params.b);
  }
};

// This is a specialized template for Callback3.
template <typename Arg1, typename Arg2, typename Arg3>
class MockCallback<void(Arg1, Arg2, Arg3)>
    : public Callback3<Arg1, Arg2, Arg3>::Type {
 public:
  MOCK_METHOD3_T(OnRun, void(Arg1 arg1, Arg2 arg2, Arg3 arg3));
  virtual void RunWithParams(const Tuple3<Arg1, Arg2, Arg3> &params) {
    OnRun(params.a, params.b, params.c);
  }
};

// This is a specialized template for Callback4.
template <typename Arg1, typename Arg2, typename Arg3, typename Arg4>
class MockCallback<void(Arg1, Arg2, Arg3, Arg4)>
    : public Callback4<Arg1, Arg2, Arg3, Arg4>::Type {
 public:
  MOCK_METHOD4_T(OnRun, void(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4));
  virtual void RunWithParams(const Tuple4<Arg1, Arg2, Arg3, Arg4> &params) {
    OnRun(params.a, params.b, params.c, params.d);
  }
};

// This is a specialized template for Callback5.
template <typename Arg1, typename Arg2, typename Arg3, typename Arg4,
          typename Arg5>
class MockCallback<void(Arg1, Arg2, Arg3, Arg4, Arg5)>
    : public Callback5<Arg1, Arg2, Arg3, Arg4, Arg5>::Type {
 public:
  MOCK_METHOD5_T(OnRun, void(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4,
                             Arg5 arg5));
  virtual void RunWithParams(
      const Tuple5<Arg1, Arg2, Arg3, Arg4, Arg5> &params) {
    OnRun(params.a, params.b, params.c, params.d, params.e);
  }
};

// NewMockCallback creates the various MockCallbacks.  It uses the functor
// NewMockCallbackImpl.
namespace internal {

class NewMockCallbackImpl {
 public:
  template <typename F>
  operator MockCallback<F> *() const {
    return new MockCallback<F>;
  }
};

}  // namespace internal

inline internal::NewMockCallbackImpl NewMockCallback() {
  return internal::NewMockCallbackImpl();
}

}  // namespace shill

#endif  // SHILL_MOCK_CALLBACK_
