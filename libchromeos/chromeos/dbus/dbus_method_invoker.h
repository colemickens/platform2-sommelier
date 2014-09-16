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
// Returns a dbus::Response object on success. On failure, returns nullptr and
// fills in additional error details into the |error| object.
template<typename... Args>
inline std::unique_ptr<dbus::Response> CallMethodAndBlockWithTimeout(
    int timeout_ms,
    dbus::ObjectProxy* object,
    const std::string& interface_name,
    const std::string& method_name,
    ErrorPtr* error,
    const Args&... args) {
  dbus::MethodCall method_call(interface_name, method_name);
  // Add method arguments to the message buffer.
  dbus::MessageWriter writer(&method_call);
  DBusParamWriter::Append(&writer, args...);
  auto response = object->CallMethodAndBlock(&method_call, timeout_ms);
  if (!response) {
    Error::AddToPrintf(error, errors::dbus::kDomain, DBUS_ERROR_FAILED,
                       "Failed to call D-Bus method: %s.%s",
                       interface_name.c_str(),
                       method_name.c_str());
  }
  return std::unique_ptr<dbus::Response>(response.release());
}

// Same as CallMethodAndBlockWithTimeout() but uses a default timeout value.
template<typename... Args>
inline std::unique_ptr<dbus::Response> CallMethodAndBlock(
    dbus::ObjectProxy* object,
    const std::string& interface_name,
    const std::string& method_name,
    ErrorPtr* error,
    const Args&... args) {
  return CallMethodAndBlockWithTimeout(dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                       object,
                                       interface_name,
                                       method_name,
                                       error,
                                       args...);
}

// Extracts the parameters of |ResultTypes...| types from the message reader
// and puts the values in the resulting |tuple|. Returns false on error and
// provides additional error details in |error| object.
template<typename... ResultTypes>
inline bool ExtractMessageParametersAsTuple(
    dbus::MessageReader* reader,
    ErrorPtr* error,
    std::tuple<ResultTypes...>* val_tuple) {
  auto callback = [val_tuple](const ResultTypes&... params) {
    *val_tuple = std::tie(params...);
  };
  return DBusParamReader<ResultTypes...>::Invoke(callback, reader, error);
}
// Overload of ExtractMessageParametersAsTuple to handle reference types in
// tuples created with std::tie().
template<typename... ResultTypes>
inline bool ExtractMessageParametersAsTuple(
    dbus::MessageReader* reader,
    ErrorPtr* error,
    std::tuple<ResultTypes&...>* ref_tuple) {
  auto callback = [ref_tuple](const ResultTypes&... params) {
    *ref_tuple = std::tie(params...);
  };
  return DBusParamReader<ResultTypes...>::Invoke(callback, reader, error);
}

// A helper method to extract a list of values from a message buffer.
// The function will return false and provide detailed error information on
// failure. It fails if the D-Bus message buffer (represented by the |reader|)
// contains too many, too few parameters or the parameters are of wrong types
// (signatures).
// The usage pattern is as follows:
//
//  int32_t data1;
//  std::string data2;
//  ErrorPtr error;
//  if (ExtractMessageParameters(reader, &error, &data1, &data2)) { ... }
//
// The above example extracts an Int32 and a String from D-Bus message buffer.
template<typename... ResultTypes>
inline bool ExtractMessageParameters(dbus::MessageReader* reader,
                                     ErrorPtr* error,
                                     ResultTypes*... results) {
  auto ref_tuple = std::tie(*results...);
  return ExtractMessageParametersAsTuple<ResultTypes...>(reader, error,
                                                         &ref_tuple);
}

// Convenient helper method to extract return value(s) of a D-Bus method call.
// |results| must be zero or more pointers to data expected to be returned
// from the method called. If an error occurs, returns false and provides
// additional details in |error| object.
//
// It is OK to call this method even if the D-Bus method doesn't expect
// any return values. Just do not specify any output |results|. In this case,
// ExtractMethodCallResults() will verify that the method didn't return any
// data in the |message|.
template<typename... ResultTypes>
inline bool ExtractMethodCallResults(dbus::Message* message,
                                     ErrorPtr* error,
                                     ResultTypes*... results) {
  CHECK(message) << "Unable to extract parameters from a NULL message.";

  dbus::MessageReader reader(message);
  return ExtractMessageParameters(&reader, error, results...);
}

}  // namespace dbus_utils
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_DBUS_DBUS_METHOD_INVOKER_H_
