//
// Copyright (C) 2013 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
