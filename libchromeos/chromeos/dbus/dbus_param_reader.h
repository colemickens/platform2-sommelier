// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides generic method to parse function call arguments from
// D-Bus message buffer and subsequently invokes a provided native C++ callback
// with the parameter values passed as the callback arguments.

// This functionality is achieved by parsing method arguments one by one,
// left to right from the C++ callback's type signature, and moving the parsed
// arguments to the back to the next call to DBusInvoke::Invoke's arguments as
// const refs.  Each iteration has one fewer template specialization arguments,
// until there is only the return type remaining and we fall through to either
// the void or the non-void final specialization.

#ifndef LIBCHROMEOS_CHROMEOS_DBUS_DBUS_PARAM_READER_H_
#define LIBCHROMEOS_CHROMEOS_DBUS_DBUS_PARAM_READER_H_

#include <type_traits>

#include <chromeos/dbus/data_serialization.h>
#include <chromeos/dbus/utils.h>
#include <chromeos/errors/error.h>
#include <chromeos/errors/error_codes.h>
#include <dbus/message.h>

namespace chromeos {
namespace dbus_utils {

// A generic DBusParamReader stub class which allows us to specialize on
// a variable list of expected function parameters later on.
// This struct in itself is not used. But its concrete template specializations
// defined below are.
template<typename...>
struct DBusParamReader;

// A generic specialization of DBusParamReader to handle variable function
// parameters. This specialization pops one parameter off the D-Bus message
// buffer and calls other specializations of DBusParamReader with fewer
// parameters to pop the remaining parameters.
//  ParamType     - the type of the current method parameter we are processing.
//  RestOfParams  - the types of remaining parameters to be processed.
template<typename ParamType, typename... RestOfParams>
struct DBusParamReader<ParamType, RestOfParams...> {
  // DBusParamReader::Invoke() is a member function that actually extracts the
  // current parameter from the message buffer.
  //  handler     - the C++ callback functor to be called when all the
  //                parameters are processed.
  //  method_call - D-Bus method call object we are processing.
  //  reader      - D-Bus message reader to pop the current argument value from.
  //  args...     - the callback parameters processed so far.
  template<typename CallbackType, typename... Args>
  static bool Invoke(const CallbackType& handler,
                     ErrorPtr* error,
                     dbus::MessageReader* reader,
                     const Args&... args) {
    if (!reader->HasMoreData()) {
      Error::AddTo(error, errors::dbus::kDomain, DBUS_ERROR_INVALID_ARGS,
                   "Too few parameters in a method call");
      return false;
    }
    // ParamType could be a reference type (e.g. 'const std::string&').
    // Here we need a value type so we can create an object of this type and
    // pop the value off the message buffer into. Using std::decay<> to get
    // the value type. If ParamType is already a value type, ParamValueType will
    // be the same as ParamType.
    using ParamValueType = typename std::decay<ParamType>::type;
    // The variable to hold the value of the current parameter we reading from
    // the message buffer.
    ParamValueType current_param;
    if (!PopValueFromReader(reader, &current_param)) {
      Error::AddTo(error, errors::dbus::kDomain, DBUS_ERROR_INVALID_ARGS,
                   "Method parameter type mismatch");
      return false;
    }
    // Call DBusParamReader::Invoke() to process the rest of parameters.
    // Note that this is not a recursive call because it is calling a different
    // method of a different class. We exclude the current parameter type
    // (ParamType) from DBusParamReader<> template parameter list and forward
    // all the parameters to the arguments of Invoke() and append the current
    // parameter to the end of the parameter list. We pass it as a const
    // reference to allow to use move-only types such as std::unique_ptr<> and
    // to eliminate unnecessarily copying data.
    return DBusParamReader<RestOfParams...>::Invoke(
        handler, error, reader, args...,
        static_cast<const ParamValueType&>(current_param));
  }
};  // struct DBusParamReader<ParamType, RestOfParams...>

// The final specialization of DBusParamReader<> used when no more parameters
// are expected in the message buffer. Actually dispatches the call to the
// handler with all the accumulated arguments.
template<>
struct DBusParamReader<> {
  template<typename CallbackType, typename... Args>
  static bool Invoke(const CallbackType& handler,
                     ErrorPtr* error,
                     dbus::MessageReader* reader,
                     const Args&... args) {
    if (reader->HasMoreData()) {
      Error::AddTo(error, errors::dbus::kDomain, DBUS_ERROR_INVALID_ARGS,
                   "Too many parameters in a method call");
      return false;
    }
    handler(args...);
    return true;
  }
};  // struct DBusParamReader<>

}  // namespace dbus_utils
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_DBUS_DBUS_PARAM_READER_H_
