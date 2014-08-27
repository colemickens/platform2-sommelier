// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides specialization of D-Bus data serialization routines
// for custom data types used by peerd.

#ifndef PEERD_DBUS_DATA_SERIALIZATION_H_
#define PEERD_DBUS_DATA_SERIALIZATION_H_

#include <chromeos/dbus/data_serialization.h>

#include <string>

struct sockaddr_storage;

namespace dbus {
class MessageReader;
class MessageWriter;
}  // namespace dbus

// These methods must be in chromeos::dbus_utils namespace since we are
// specializing DBusSignature template structure already defined in
// chromeos/dbus/data_serialization.h.
namespace chromeos {
namespace dbus_utils {

// Specializations/overloads to send "sockaddr_storage" structure over D-Bus.
template<> struct DBusSignature<sockaddr_storage> { static std::string get(); };
bool AppendValueToWriter(dbus::MessageWriter* writer,
                         const sockaddr_storage& value);
bool PopValueFromReader(dbus::MessageReader* reader, sockaddr_storage* value);

}  // namespace dbus_utils
}  // namespace chromeos

#endif  // PEERD_DBUS_DATA_SERIALIZATION_H_
