// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DBusParamWriter::Append(writer, ...) provides functionality opposite
// to that of DBusParamReader. It writes each of the arguments to D-Bus message
// writer and return true if successful.

#ifndef LIBCHROMEOS_CHROMEOS_DBUS_DBUS_PARAM_WRITER_H_
#define LIBCHROMEOS_CHROMEOS_DBUS_DBUS_PARAM_WRITER_H_

#include <chromeos/dbus/data_serialization.h>
#include <dbus/message.h>

namespace chromeos {
namespace dbus_utils {

struct DBusParamWriter {
  // Generic writer method that takes 1 or more arguments. It recursively calls
  // itself (each time with one fewer arguments) until no more is left.
  template<typename ParamType, typename... RestOfParams>
  static bool Append(dbus::MessageWriter* writer,
                     const ParamType& param,
                     const RestOfParams&... rest) {
    // Append the current |param| to D-Bus, then call Append() with one
    // fewer arguments, until none is left and stand-alone version of
    // Append(dbus::MessageWriter*) is called to end the iteration.
    return AppendValueToWriter(writer, param) &&
           Append(writer, rest...);
  }

// The final overload of DBusParamWriter::Append() used when no more parameters
// are remaining to be written.  Does nothing and finishes meta-recursion.
  static bool Append(dbus::MessageWriter* writer) { return true; }
};

}  // namespace dbus_utils
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_DBUS_DBUS_PARAM_WRITER_H_
