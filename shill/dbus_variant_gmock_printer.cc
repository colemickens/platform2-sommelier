// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_variant_gmock_printer.h"

#include <gmock/gmock.h>

#include "shill/dbus_adaptor.h"

namespace DBus {

void PrintTo(const ::DBus::Variant& value, ::std::ostream* os) {
  if (shill::DBusAdaptor::IsBool(value.signature()))
    *os << value.reader().get_bool();
  else if (shill::DBusAdaptor::IsByte(value.signature()))
    *os << value.reader().get_byte();
  else if (shill::DBusAdaptor::IsInt16(value.signature()))
    *os << value.reader().get_int16();
  else if (shill::DBusAdaptor::IsInt32(value.signature()))
    *os << value.reader().get_int32();
  else if (shill::DBusAdaptor::IsPath(value.signature()))
    *os << value.reader().get_path();
  else if (shill::DBusAdaptor::IsString(value.signature()))
    *os << value.reader().get_string();
  else if (shill::DBusAdaptor::IsStringmap(value.signature()))
    *os << testing::PrintToString(value.operator shill::Stringmap());
  else if (shill::DBusAdaptor::IsStringmaps(value.signature()))
    *os << testing::PrintToString(value.operator shill::Stringmaps());
  else if (shill::DBusAdaptor::IsStrings(value.signature()))
    *os << testing::PrintToString(value.operator shill::Strings());
  else if (shill::DBusAdaptor::IsUint16(value.signature()))
    *os << value.reader().get_uint16();
  else if (shill::DBusAdaptor::IsUint32(value.signature()))
    *os << value.reader().get_uint32();
  else if (shill::DBusAdaptor::IsUint64(value.signature()))
    *os << value.reader().get_uint64();
  else
    *os << "(Do not know how to print: unknown type: " << value.signature()
        << ")";
}

}  // namespace DBus
