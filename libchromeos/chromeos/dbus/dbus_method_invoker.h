// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a way to call D-Bus methods on objects in remote processes
// as if they were native C++ function calls.

// CallMethodAndBlock (along with CallMethodAndBlockWithTimeout) lets you call
// a D-Bus method and pass all the required parameters as C++ function
// arguments. CallMethodAndBlock relies on automatic C++ to D-Bus data
// serialization implemented in chromeos/dbus/data_serialization.h.
// CallMethodAndBlock invokes the D-Bus method and returns the Response.

// The method call response can (and should) be parsed with
// ExtractMethodCallResults(). The method takes an optional list of pointers
// to the expected return values of the D-Bus method.

// Here is an example of usage.
// Call "std::string MyInterface::MyMethod(int, double)" over D-Bus:

//  using chromeos::dbus_utils::CallMethodAndBlock;
//  using chromeos::dbus_utils::ExtractMethodCallResults;
//
//  auto resp = CallMethodAndBlock(obj,
//                                 "org.chromium.MyService.MyInterface",
//                                 "MyMethod",
//                                 2, 8.7);
//
//  chromeos::ErrorPtr error;
//  std::string return_value;
//  if (ExtractMethodCallResults(resp.get(), &error, &return_value)) {
//    // Use the |return_value|.
//  } else {
//    // An error occurred. Use |error| to get details.
//  }

#ifndef LIBCHROMEOS_CHROMEOS_DBUS_DBUS_METHOD_INVOKER_H_
#define LIBCHROMEOS_CHROMEOS_DBUS_DBUS_METHOD_INVOKER_H_

#include <memory>
#include <string>
#include <tuple>

#include <chromeos/dbus/dbus_param_reader.h>
#include <chromeos/dbus/dbus_param_writer.h>
#include <chromeos/errors/error.h>
#include <chromeos/errors/error_codes.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

namespace chromeos {
namespace dbus_utils {

// A helper method to dispatch a blocking D-Bus method call. Can specify
// zero or more method call arguments in |args| which will be sent over D-Bus.
// This method sends a D-Bus message and blocks for a time period specified
// in |timeout_ms| while waiting for a reply. The time out is in milliseconds or
// -1 (DBUS_TIMEOUT_USE_DEFAULT) for default, or DBUS_TIMEOUT_INFINITE for no
// timeout. If a timeout occurs, the response object contains an error object
// with DBUS_ERROR_NO_REPLY error code (those constants come from libdbus
// [dbus/dbus.h]).
template<typename... Args>
inline std::unique_ptr<dbus::Response> CallMethodAndBlockWithTimeout(
    int timeout_ms,
    dbus::ObjectProxy* object,
    const std::string& interface_name,
    const std::string& method_name,
    const Args&... args) {
  dbus::MethodCall method_call(interface_name, method_name);
  // Add method arguments to the message buffer.
  dbus::MessageWriter writer(&method_call);
  DBusParamWriter::Append(&writer, args...);
  auto response = object->CallMethodAndBlock(&method_call, timeout_ms);
  return std::unique_ptr<dbus::Response>(response.release());
}

// Same as CallMethodAndBlockWithTimeout() but uses a default timeout value.
template<typename... Args>
inline std::unique_ptr<dbus::Response> CallMethodAndBlock(
    dbus::ObjectProxy* object,
    const std::string& interface_name,
    const std::string& method_name,
    const Args&... args) {
  return CallMethodAndBlockWithTimeout(dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                       object,
                                       interface_name,
                                       method_name,
                                       args...);
}

// Convenient helper method to extract return value(s) of a D-Bus method call.
// |results| must be zero or more pointers to data expected to be returned
// from the method called. If an error occurs, returns false and provides
// additional details in |error| object.
//
// It is OK to call this method even if the D-Bus method doesn't expect
// any returned values. Just do not specify any output |results|. In this case,
// ExtractMethodCallResults() will verify that the method didn't return any
// data in the response message.
//
// If |response| contains an error message, it is extracted and set to |error|
// object and ExtractMethodCallResults() will return false.
template<typename... ResultTypes>
inline bool ExtractMethodCallResults(dbus::Response* response,
                                     ErrorPtr* error,
                                     ResultTypes*... results) {
  dbus::MessageReader reader(response);
  // If response contain an error message, extract the error information
  // into the |error| object.
  if (response->GetMessageType() == dbus::Message::MESSAGE_ERROR) {
    dbus::ErrorResponse* error_response =
        static_cast<dbus::ErrorResponse*>(response);
    std::string error_code = error_response->GetErrorName();
    std::string error_message;
    reader.PopString(&error_message);
    Error::AddTo(error, errors::dbus::kDomain, error_code, error_message);
    return false;
  }
  // gcc 4.8 does not allow to pass the parameter packs via lambda captures
  // so using a tuple instead. Tie the results to a tuple and pass the tuple
  // to the lambda callback.
  // Apparently this is fixed in gcc 4.9, but this is a reasonable work-around
  // and might be used after upgrade to v4.9.
  // |results_ref_tuple| will contain lvalue references to the output objects
  // passed in as pointers in arguments to ExtractMethodCallResults().
  auto results_ref_tuple = std::tie(*results...);
  auto callback = [&results_ref_tuple](const ResultTypes&... params) {
    results_ref_tuple = std::tie(params...);
  };
  return DBusParamReader<ResultTypes...>::Invoke(callback, error, &reader);
}

}  // namespace dbus_utils
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_DBUS_DBUS_METHOD_INVOKER_H_
