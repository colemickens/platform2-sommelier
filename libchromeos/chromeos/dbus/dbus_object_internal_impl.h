// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides internal implementation details of dispatching DBus
// method calls to a generic callback by reading the expected parameter values
// from DBus message buffer until all the values are read and then
// native C++ callback is called with those parameters passed in. If the
// callback returns a value, that value is sent back to the caller of DBus
// method via the response message.

// This functionality is achieved by parsing method arguments one by one,
// left to right from the C++ callback's type signature, and moving the parsed
// arguments to the back to the next call to DBusInvoke::Invoke's arguments as
// const refs.  Each iteration has one fewer template specialization arguments,
// until there is only the return type remaining and we fall through to either
// the void or the non-void final specialization.

#ifndef LIBCHROMEOS_CHROMEOS_DBUS_DBUS_OBJECT_INTERNAL_IMPL_H_
#define LIBCHROMEOS_CHROMEOS_DBUS_DBUS_OBJECT_INTERNAL_IMPL_H_

#include <memory>
#include <string>
#include <type_traits>

#include <chromeos/dbus_utils.h>
#include <chromeos/error.h>
#include <dbus/message.h>

namespace chromeos {
namespace dbus_utils {

// A generic DBusInvoker stub class which allows us to specialize on a variable
// list of expected function parameters later on. This struct in itself is not
// used. But its concrete template specializations are.
template<typename ReturnType, typename...>
struct DBusInvoker;

// A generic specialization of DBusInvoker to handle variable function
// parameters. This specialization pops one parameter off the DBus message
// buffer and calls other specializations of DBusInvoker with fewer parameters
// to pop the remaining parameters.
//  ReturnType    - the return type of the callback we expect to call.
//  ParamType     - the type of the current method parameter we are processing.
//  RestOfParams  - the types of remaining parameters to be processed.
template<typename ReturnType, typename ParamType, typename... RestOfParams>
struct DBusInvoker<ReturnType, ParamType, RestOfParams...> {
  // DBusInvoker::Invoke() is a member function that actually extracts the
  // current parameter from the message buffer.
  //  handler     - the C++ callback to be called when all the parameters are
  //                processed.
  //  method_call - DBus method call object we are processing.
  //  reader      - DBus message reader to pop the current parameter value from.
  //  args...     - the callback parameters processed so far.
  template<typename CallbackType, typename... Args>
  static std::unique_ptr<dbus::Response> Invoke(const CallbackType& handler,
                                                dbus::MethodCall* method_call,
                                                dbus::MessageReader* reader,
                                                const Args&... args) {
    if (!reader->HasMoreData())
      return CreateDBusErrorResponse(method_call,
                                     DBUS_ERROR_INVALID_ARGS,
                                     "Too few parameters in a method call");
    // ParamType could be a reference type (e.g. 'const std::string&').
    // Here we need a value type so we can create an object of this type and
    // pop the value off the message buffer into. Using std::decay<> to get
    // the value type. If ParamType is already a value type, ParamValueType will
    // be the same as ParamType.
    using ParamValueType = typename std::decay<ParamType>::type;
    // The variable to hold the value of the current parameter we reading from
    // the message buffer.
    ParamValueType current_param;
    if (!chromeos::dbus_utils::PopValueFromReader(reader, &current_param))
      return CreateDBusErrorResponse(method_call,
                                     DBUS_ERROR_INVALID_ARGS,
                                     "Method parameter type mismatch");
    // Call DBusInvoker::Invoke() to process the rest of parameters.
    // Note that this is not a recursive call because it is calling a different
    // method of a different class. We exclude the current parameter type
    // (ParamType) from DBusInvoker<> template parameter list and forward all
    // the parameters to the arguments of Invoke() and append the current
    // parameter to the end of the parameter list. We pass it as a const
    // reference to allow to use move-only types such as std::unique_ptr<> and
    // to eliminate unnecessarily copying data.
    return DBusInvoker<ReturnType, RestOfParams...>::Invoke(
        handler, method_call, reader, args...,
        static_cast<const ParamValueType&>(current_param));
  }
};  // struct DBusInvoker<ReturnType, ParamType, RestOfParams...>

// This is a final specialization of DBusInvoker<> that actually calls the
// C++ callback methods with all the accumulated arguments. This specialization
// expects that the callback returns a non-void value that needs to be
// sent back to the caller via DBus.
template<typename ReturnType>
struct DBusInvoker<ReturnType> {
  // This specialization of DBusInvoker::Invoke() actually calls |handler|
  // which is expected to be of base::Callback<ReturnType(ErrorPtr*, Args...)>
  // type.
  template<typename CallbackType, typename... Args>
  static std::unique_ptr<dbus::Response> Invoke(const CallbackType& handler,
                                                dbus::MethodCall* method_call,
                                                dbus::MessageReader* reader,
                                                const Args&... args) {
    if (reader->HasMoreData())
      return CreateDBusErrorResponse(method_call,
                                     DBUS_ERROR_INVALID_ARGS,
                                     "Too many parameters in a method call");
    chromeos::ErrorPtr error;
    // If |handler| fails it should provide the error information in |error|.
    ReturnType ret = handler.Run(&error, args...);
    if (error)
      return dbus_utils::GetDBusError(method_call, error.get());
    // Send back the |ret| value through DBus.
    auto response = dbus::Response::FromMethodCall(method_call);
    dbus::MessageWriter writer(response.get());
    chromeos::dbus_utils::AppendValueToWriter(&writer, ret);
    return std::unique_ptr<dbus::Response>(response.release());
  }
};  // struct DBusInvoker<ReturnType>

// This is a final specialization of DBusInvoker<> that actually calls the
// C++ callback methods with all the accumulated arguments. This specialization
// expects that the callback returns a custom std::unique_ptr<dbus::Response>
// response object in cases of both success and failure.
template<>
struct DBusInvoker<std::unique_ptr<dbus::Response>> {
  // This specialization of DBusInvoker::Invoke() actually calls |handler|
  // which is expected to be of
  // base::Callback<std::unique_ptr<dbus::Response>(dbus::MethodCall*, Args...)>
  // type.
  template<typename CallbackType, typename... Args>
  static std::unique_ptr<dbus::Response> Invoke(const CallbackType& handler,
                                                dbus::MethodCall* method_call,
                                                dbus::MessageReader* reader,
                                                const Args&... args) {
    if (reader->HasMoreData())
      return CreateDBusErrorResponse(method_call,
                                     DBUS_ERROR_INVALID_ARGS,
                                     "Too many parameters in a method call");
    return handler.Run(method_call, args...);
  }
};  // struct DBusInvoker<std::unique_ptr<dbus::Response>>

// This specialization of DBusInvoker<> is for the case where the C++ handler
// does not return any value (that is, it returns 'void'). The implementation
// of this class and its Invoke() method is pretty much identical to the above
// but without sending the return value back to the caller.
template<>
struct DBusInvoker<void> {
  template<typename CallbackType, typename... Args>
  static std::unique_ptr<dbus::Response> Invoke(const CallbackType& handler,
                                                dbus::MethodCall* method_call,
                                                dbus::MessageReader* reader,
                                                const Args&... args) {
    if (reader->HasMoreData())
      return CreateDBusErrorResponse(method_call,
                                     DBUS_ERROR_INVALID_ARGS,
                                     "Too many parameters in a method call");
    chromeos::ErrorPtr error;
    handler.Run(&error, args...);
    if (error)
      return dbus_utils::GetDBusError(method_call, error.get());
    return std::unique_ptr<dbus::Response>(
        dbus::Response::FromMethodCall(method_call).release());
  }
};  // struct DBusInvoker<void>

// This is a helper function that calls a C++ |handler| callback by reading
// the function arguments from the message buffer provided in DBus
// |method_call|. Returns a DBus Response class with the response message
// containing either a valid reply message or an error message if the call
// failed.
template<typename ReturnType, typename... Args>
std::unique_ptr<dbus::Response> CallDBusMethodHandler(
    const base::Callback<ReturnType(chromeos::ErrorPtr*, Args...)>& handler,
    dbus::MethodCall* method_call) {
  dbus::MessageReader reader(method_call);
  return DBusInvoker<ReturnType, Args...>::Invoke(handler, method_call,
                                                  &reader);
}

// CallDBusMethodHandler overload for dispatching a callback handler that
// returns a custom response object as std::unique_ptr<dbus::Response>.
template<typename... Args>
std::unique_ptr<dbus::Response> CallDBusMethodHandler(
    const base::Callback<std::unique_ptr<dbus::Response>(dbus::MethodCall*,
                                                         Args...)>& handler,
    dbus::MethodCall* method_call) {
  dbus::MessageReader reader(method_call);
  return DBusInvoker<std::unique_ptr<dbus::Response>, Args...>::Invoke(
      handler, method_call, &reader);
}

}  // namespace dbus_utils
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_DBUS_DBUS_OBJECT_INTERNAL_IMPL_H_
