// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_LIBNEWBLUE_GENERATED_FUNCTION_H_
#define BLUETOOTH_NEWBLUED_LIBNEWBLUE_GENERATED_FUNCTION_H_

// Internal implementation - Don't use in user code.
#define LIBNEWBLUE_RESULT_(func_type) \
  bluetooth::LibNewblueFunction<func_type>::Result

// Internal implementation - Don't use in user code.
#define LIBNEWBLUE_ARG_(n, func_type) \
  bluetooth::LibNewblueFunction<func_type>::Argument##n

// Macros to generate libnewblue functions wrapped in a C++ virtual methods.
// Use LIBNEWBLUE_METHODX for method with X parameters.
//
// Usage example:
// LIBNEWBLUE_METHOD2(SomeMethod, someFunction, bool(int, char));
// will produce:
// virtual bool SomeMethod(int a1, char a2) {
//   return someFunction(a1, a2);
// }

#define LIBNEWBLUE_METHOD0(cpp_name, c_name, func_type) \
  virtual LIBNEWBLUE_RESULT_(func_type) cpp_name() { return c_name(); }

#define LIBNEWBLUE_METHOD1(cpp_name, c_name, func_type) \
  virtual LIBNEWBLUE_RESULT_(func_type)                 \
      cpp_name(LIBNEWBLUE_ARG_(1, func_type) a1) {      \
    return c_name(a1);                                  \
  }

#define LIBNEWBLUE_METHOD2(cpp_name, c_name, func_type)                     \
  virtual LIBNEWBLUE_RESULT_(func_type) cpp_name(                           \
      LIBNEWBLUE_ARG_(1, func_type) a1, LIBNEWBLUE_ARG_(2, func_type) a2) { \
    return c_name(a1, a2);                                                  \
  }

#define LIBNEWBLUE_METHOD3(cpp_name, c_name, func_type)                   \
  virtual LIBNEWBLUE_RESULT_(func_type) cpp_name(                         \
      LIBNEWBLUE_ARG_(1, func_type) a1, LIBNEWBLUE_ARG_(2, func_type) a2, \
      LIBNEWBLUE_ARG_(3, func_type) a3) {                                 \
    return c_name(a1, a2, a3);                                            \
  }

#define LIBNEWBLUE_METHOD4(cpp_name, c_name, func_type)                     \
  virtual LIBNEWBLUE_RESULT_(func_type) cpp_name(                           \
      LIBNEWBLUE_ARG_(1, func_type) a1, LIBNEWBLUE_ARG_(2, func_type) a2,   \
      LIBNEWBLUE_ARG_(3, func_type) a3, LIBNEWBLUE_ARG_(4, func_type) a4) { \
    return c_name(a1, a2, a3, a4);                                          \
  }

#define LIBNEWBLUE_METHOD5(cpp_name, c_name, func_type)                   \
  virtual LIBNEWBLUE_RESULT_(func_type) cpp_name(                         \
      LIBNEWBLUE_ARG_(1, func_type) a1, LIBNEWBLUE_ARG_(2, func_type) a2, \
      LIBNEWBLUE_ARG_(3, func_type) a3, LIBNEWBLUE_ARG_(4, func_type) a4, \
      LIBNEWBLUE_ARG_(5, func_type) a5) {                                 \
    return c_name(a1, a2, a3, a4, a5);                                    \
  }

namespace bluetooth {

template <typename F>
struct LibNewblueFunction;

template <typename R>
struct LibNewblueFunction<R()> {
  using Result = R;
};

template <typename R, typename A1>
struct LibNewblueFunction<R(A1)> : LibNewblueFunction<R()> {
  using Argument1 = A1;
};

template <typename R, typename A1, typename A2>
struct LibNewblueFunction<R(A1, A2)> : LibNewblueFunction<R(A1)> {
  using Argument2 = A2;
};

template <typename R, typename A1, typename A2, typename A3>
struct LibNewblueFunction<R(A1, A2, A3)> : LibNewblueFunction<R(A1, A2)> {
  using Argument3 = A3;
};

template <typename R, typename A1, typename A2, typename A3, typename A4>
struct LibNewblueFunction<R(A1, A2, A3, A4)>
    : LibNewblueFunction<R(A1, A2, A3)> {
  using Argument4 = A4;
};

template <typename R,
          typename A1,
          typename A2,
          typename A3,
          typename A4,
          typename A5>
struct LibNewblueFunction<R(A1, A2, A3, A4, A5)>
    : LibNewblueFunction<R(A1, A2, A3, A4)> {
  using Argument5 = A5;
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_LIBNEWBLUE_GENERATED_FUNCTION_H_
