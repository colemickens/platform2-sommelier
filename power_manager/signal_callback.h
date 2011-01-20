// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_SIGNAL_CALLBACK_H_
#define POWER_MANAGER_SIGNAL_CALLBACK_H_

// These macros provide a wrapper around class methods so they can be used as
// callbacks.

// SIGNAL_CALLBACK_0(CLASS, RETURN, METHOD)
//
// Use this to invoke an object's method that takes no arguments.
//   CLASS    The object's class
//   RETURN   The return value of the method to be invoked
//   METHOD   The name of the method to be invoked
//
// Usage example:
//   class MyClass {
//   public:
//     void Run();
//   private:
//     SIGNAL_CALLBACK_0(MyClass, bool, CallbackFunc);
//   };
//   bool MyClass::CallbackFunc() {
//     // Do something.
//   }
//   bool MyClass::Run() {
//     // Will result in this->CallbackFunc() being invoked.
//     g_timeout_add(0, MyClass::CallbackFuncThunk, this);
//   }

#define SIGNAL_CALLBACK_0(CLASS, RETURN, METHOD)      \
  static RETURN METHOD ## Thunk(void* data) {         \
    return reinterpret_cast<CLASS*>(data)->METHOD();  \
  }                                                   \
  RETURN METHOD();

// SIGNAL_CALLBACK_[n](CLASS, RETURN, METHOD, TYPE0, TYPE1, ...)
//
// Use this to invoke an object's method that take one or more arguments.
//   CLASS    The object's class
//   RETURN   The return value of the method to be invoked
//   METHOD   The name of the method to be invoked
//   TYPE[n]  The types of the arguments
//
// Usage example:
//   class MyClass {
//   public:
//     void Run();
//   private:
//     SIGNAL_CALLBACK_2(MyClass, bool, CallbackFunc, int, char*);
//   };
//   bool MyClass::CallbackFunc(int a, char* b) {
//     // Do something.
//   }
//   bool MyClass::Run() {
//     int value;
//     char* string;
//     // Will result in this->CallbackFunc(value, string) being invoked.
//     g_timeout_add(0, MyClass::CallbackFuncThunk,
//                   CreateCallbackFuncArgs(this, value, string));
//   }

#define SIGNAL_CALLBACK_1(CLASS, RETURN, METHOD, TYPE0)              \
  struct METHOD ## Args {                                            \
    CLASS* obj;                                                      \
    TYPE0 arg0;                                                      \
  };                                                                 \
  static METHOD ## Args* Create ## METHOD ## Args(CLASS* obj,        \
                                                  TYPE0& arg0) {     \
    METHOD ## Args* args = new METHOD ## Args;                       \
    args->obj = obj;                                                 \
    args->arg0 = arg0;                                               \
    return args;                                                     \
  }                                                                  \
  static RETURN METHOD ## Thunk(void* data) {                        \
    METHOD ## Args* args = reinterpret_cast<METHOD ## Args*>(data);  \
    CLASS* obj = args->obj;                                          \
    TYPE0 arg0 = args->arg0;                                         \
    delete args;                                                     \
    return obj->METHOD(arg0);                                        \
  }                                                                  \
  RETURN METHOD(TYPE0);

#define SIGNAL_CALLBACK_2(CLASS, RETURN, METHOD, TYPE0, TYPE1)       \
  struct METHOD ## Args {                                            \
    CLASS* obj;                                                      \
    TYPE0 arg0;                                                      \
    TYPE1 arg1;                                                      \
  };                                                                 \
  static METHOD ## Args* Create ## METHOD ## Args(CLASS* obj,        \
                                                  TYPE0& arg0,       \
                                                  TYPE1& arg1) {     \
    METHOD ## Args* args = new METHOD ## Args;                       \
    args->obj = obj;                                                 \
    args->arg0 = arg0;                                               \
    args->arg1 = arg1;                                               \
    return args;                                                     \
  }                                                                  \
  static RETURN METHOD ## Thunk(void* data) {                        \
    METHOD ## Args* args = reinterpret_cast<METHOD ## Args*>(data);  \
    CLASS* obj = args->obj;                                          \
    TYPE0 arg0 = args->arg0;                                         \
    TYPE0 arg1 = args->arg1;                                         \
    delete args;                                                     \
    return obj->METHOD(arg0, arg1);                                  \
  }                                                                  \
  RETURN METHOD(TYPE0, TYPE1);

#endif  // POWER_MANAGER_SIGNAL_CALLBACK_H_
