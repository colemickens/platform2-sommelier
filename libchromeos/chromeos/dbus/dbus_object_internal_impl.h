// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides internal implementation details of dispatching D-Bus
// method calls to a D-Bus object methods by reading the expected parameter
// values from D-Bus message buffer then invoking a native C++ callback with
// those parameters passed in. If the callback returns a value, that value is
// sent back to the caller of D-Bus method via the response message.

// This is achieved by redirecting the parsing of parameter values from D-Bus
// message buffer to DBusParamReader helper class.
// DBusParamReader de-serializes the parameter values from the D-Bus message
// and calls the provided native C++ callback with those arguments.
// However it expects the callback with a simple signature like this:
//    void callback(Args...);
// The method handlers for DBusObject, on the other hand, have one of the two
// possible signatures:
//    ReturnType handler(ErrorPtr*, Args...);
//    unique_ptr<dbus::Response> handler(dbus::MethodCall*, Args...);
//
// To make this all work, we craft a simple callback suitable for
// DBusParamReader using a lambda in DBusInvoker::Invoke() and redirect the call
// to the appropriate method handler using additional data captured by the
// lambda object.

#ifndef LIBCHROMEOS_CHROMEOS_DBUS_DBUS_OBJECT_INTERNAL_IMPL_H_
#define LIBCHROMEOS_CHROMEOS_DBUS_DBUS_OBJECT_INTERNAL_IMPL_H_

#include <memory>
#include <string>
#include <type_traits>

#include <chromeos/dbus/dbus_param_reader.h>
#include <chromeos/dbus_utils.h>
#include <chromeos/error.h>
#include <dbus/message.h>

namespace chromeos {
namespace dbus_utils {

// Possible method handler signature types:
template<typename ReturnType, typename... Args>
using TypedReturnDBusMethodHandler =
  base::Callback<ReturnType(chromeos::ErrorPtr*, Args...)>;

template<typename... Args>
using RawReturnDBusMethodHandler =
  base::Callback<std::unique_ptr<dbus::Response>(dbus::MethodCall*, Args...)>;


// A generic TypedReturnDBusInvoker class to invoke the handler and provide the
// parameters whose values are read from the D-Bus message handler.
// Template arguments are:
//  ReturnType    - the return type of the handler we expect to call.
//  CallbackType  - signature (type) of the handler which must be:
//                  base::Callback<ReturnType(ErrorPtr*, Args...)>;
//  Args          - types of arguments to read from the D-Bus message and
//                  pass to the handler.
template<typename ReturnType, typename CallbackType, typename... Args>
struct TypedReturnDBusInvoker {
  // TypedReturnDBusInvoker::Invoke() is a member function that actually
  // extracts the parameters from the message buffer and calls the handler.
  //  handler     - the C++ callback to be called after all the parameters are
  //                read from the message.
  //  method_call - D-Bus method call object we are processing.
  //  reader      - D-Bus message reader to parse the parameter values from.
  static std::unique_ptr<dbus::Response> Invoke(const CallbackType& handler,
                                                dbus::MethodCall* method_call,
                                                dbus::MessageReader* reader) {
    // This is a generic method and the handler is expected to return a
    // (non-void) return value. It can also return an error via the first
    // argument of type ErrorPtr*.
    // Create a simple callback wrapper around the method handler to hide these
    // extra arguments from DBusParamReader<Args...>::Invoke() by capturing
    // them in the lambda objects and then using them to call the real handler.
    ErrorPtr handler_error;
    ReturnType handler_retval;
    auto param_reader_callback =
        [&handler_error, &handler_retval, &handler](const Args&... args) {
      handler_retval = handler.Run(&handler_error, args...);
    };

    // DBusParamReader<Args...>::Invoke() may return its own error when parsing
    // arguments, so give it another instance of ErrorPtr.
    ErrorPtr param_reader_error;
    if (!DBusParamReader<Args...>::Invoke(param_reader_callback,
                                          &param_reader_error,
                                          reader)) {
      // Parsing the handler arguments failed. Return D-Bus error.
      return GetDBusError(method_call, param_reader_error.get());
    }

    // If |handler| failed, return the error information stored in
    // |handler_error| object.
    if (handler_error)
      return dbus_utils::GetDBusError(method_call, handler_error.get());

    // Send back the |handler_retval| value through D-Bus.
    auto response = dbus::Response::FromMethodCall(method_call);
    dbus::MessageWriter writer(response.get());
    chromeos::dbus_utils::AppendValueToWriter(&writer, handler_retval);
    return std::unique_ptr<dbus::Response>(response.release());
  }
};  // struct TypedReturnDBusInvoker<ReturnType, ...>

// This is a specialization of TypedReturnDBusInvoker<> for a method handler
// which is not expected to return any value (ReturnType = void).
// So the |handler| signature must be:
//   base::Callback<void(ErrorPtr*, Args...)>;
template<typename CallbackType, typename... Args>
struct TypedReturnDBusInvoker<void, CallbackType, Args...> {
  // Implementation of Invoke() is identical to the one above except there is no
  // return value from the method handler to send back to the D-Bus method
  // caller.
  static std::unique_ptr<dbus::Response> Invoke(const CallbackType& handler,
                                                dbus::MethodCall* method_call,
                                                dbus::MessageReader* reader) {
    ErrorPtr handler_error;
    auto param_reader_callback =
        [&handler_error, &handler](const Args&... args) {
      handler.Run(&handler_error, args...);
    };

    ErrorPtr param_reader_error;
    if (!DBusParamReader<Args...>::Invoke(param_reader_callback,
                                          &param_reader_error,
                                          reader)) {
      // Error parsing method arguments.
      return GetDBusError(method_call, param_reader_error.get());
    }

    // Method handler returned an error.
    if (handler_error)
      return dbus_utils::GetDBusError(method_call, handler_error.get());

    return std::unique_ptr<dbus::Response>(
        dbus::Response::FromMethodCall(method_call).release());
  }
};  // struct TypedReturnDBusInvoker<void, ...>

// RawReturnDBusInvoker is similar to TypedReturnDBusInvoker<> above but there
// is no special error handling or typed return values from the handler.
// Instead, the handler returns the raw response message which could be
// a method call reply or an error response.
// The |handler| signature must be:
//   base::Callback<std::unique_ptr<dbus::Response>(dbus::MethodCall*, Args...)>
template<typename CallbackType, typename... Args>
struct RawReturnDBusInvoker {
  static std::unique_ptr<dbus::Response> Invoke(const CallbackType& handler,
                                                dbus::MethodCall* method_call,
                                                dbus::MessageReader* reader) {
    std::unique_ptr<dbus::Response> handler_response;
    auto param_reader_callback =
        [&handler_response, method_call, &handler](const Args&... args) {
      handler_response = handler.Run(method_call, args...);
    };

    ErrorPtr param_reader_error;
    if (!DBusParamReader<Args...>::Invoke(param_reader_callback,
                                          &param_reader_error,
                                          reader)) {
      return GetDBusError(method_call, param_reader_error.get());
    }
    return handler_response;
  }
};  // struct RawReturnDBusInvoker

// This is a helper function that calls a C++ |handler| callback by reading
// the function arguments from the message buffer provided in D-Bus
// |method_call|. Returns a D-Bus Response class with the response message
// containing either a valid reply message or an error message if the call
// failed.
template<typename ReturnType, typename... Args>
std::unique_ptr<dbus::Response> CallDBusMethodHandler(
    const TypedReturnDBusMethodHandler<ReturnType, Args...>& handler,
    dbus::MethodCall* method_call) {
  dbus::MessageReader reader(method_call);
  using Handler = TypedReturnDBusMethodHandler<ReturnType, Args...>;
  return TypedReturnDBusInvoker<ReturnType, Handler, Args...>::Invoke(handler,
                                                                method_call,
                                                                &reader);
}

// CallDBusMethodHandler overload for dispatching a callback handler that
// returns a custom response object as std::unique_ptr<dbus::Response>.
template<typename... Args>
std::unique_ptr<dbus::Response> CallDBusMethodHandler(
    const RawReturnDBusMethodHandler<Args...>& handler,
    dbus::MethodCall* method_call) {
  dbus::MessageReader reader(method_call);
  using Handler = RawReturnDBusMethodHandler<Args...>;
  return RawReturnDBusInvoker<Handler, Args...>::Invoke(handler,
                                                        method_call,
                                                        &reader);
}

}  // namespace dbus_utils
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_DBUS_DBUS_OBJECT_INTERNAL_IMPL_H_
