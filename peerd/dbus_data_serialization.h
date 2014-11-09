// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides specialization of D-Bus data serialization routines
// for custom data types used by peerd.

#ifndef PEERD_DBUS_DATA_SERIALIZATION_H_
#define PEERD_DBUS_DATA_SERIALIZATION_H_

#include <string>

#include <chromeos/dbus/data_serialization.h>

namespace peerd {
struct ip_addr;
}

namespace dbus {
class MessageReader;
class MessageWriter;
}  // namespace dbus

// These methods must be in chromeos::dbus_utils namespace since we are
// specializing DBusType template structure already defined in
// chromeos/dbus/data_serialization.h.
namespace chromeos {
namespace dbus_utils {

void AppendValueToWriter(dbus::MessageWriter* writer,
                         const peerd::ip_addr& value);
bool PopValueFromReader(dbus::MessageReader* reader, peerd::ip_addr* value);

// Specializations/overloads to send "sockaddr_storage" structure over D-Bus.
template<> struct DBusType<peerd::ip_addr> {
  static std::string GetSignature();
  inline static void Write(dbus::MessageWriter* writer,
                           const peerd::ip_addr& value) {
    AppendValueToWriter(writer, value);
  }
  inline static bool Read(dbus::MessageReader* reader,
                          peerd::ip_addr* value) {
    return PopValueFromReader(reader, value);
  }
};

}  // namespace dbus_utils
}  // namespace chromeos

#endif  // PEERD_DBUS_DATA_SERIALIZATION_H_
