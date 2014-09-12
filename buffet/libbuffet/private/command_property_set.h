// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_LIBBUFFET_PRIVATE_COMMAND_PROPERTY_SET_H_
#define BUFFET_LIBBUFFET_PRIVATE_COMMAND_PROPERTY_SET_H_

#include <string>

#include <base/macros.h>
#include <chromeos/any.h>
#include <chromeos/dbus/data_serialization.h>
#include <dbus/object_manager.h>

#include "buffet/libbuffet/export.h"

namespace dbus {
// Specialize dbus::Property for chromeos::dbus_utils::Dictionary type.
template class Property<chromeos::dbus_utils::Dictionary>;

template <>
inline bool Property<chromeos::dbus_utils::Dictionary>::PopValueFromReader(
    MessageReader* reader) {
  return chromeos::dbus_utils::PopVariantValueFromReader(reader, &value_);
}

template <>
inline void Property<chromeos::dbus_utils::Dictionary>::AppendSetValueToWriter(
    MessageWriter* writer) {
  chromeos::dbus_utils::AppendValueToWriterAsVariant(writer, set_value_);
}
}  // namespace dbus

namespace buffet {

// PropertySet for remote D-Bus GCD Command object from Buffet daemon.
class CommandPropertySet : public dbus::PropertySet {
 public:
  CommandPropertySet(dbus::ObjectProxy* object_proxy,
                     const std::string& interface_name,
                     const PropertyChangedCallback& callback);
  dbus::Property<std::string> id;
  dbus::Property<std::string> name;
  dbus::Property<std::string> category;
  dbus::Property<std::string> status;
  dbus::Property<int32_t> progress;
  dbus::Property<chromeos::dbus_utils::Dictionary> parameters;

 private:
  DISALLOW_COPY_AND_ASSIGN(CommandPropertySet);
};

}  // namespace buffet

#endif  // BUFFET_LIBBUFFET_PRIVATE_COMMAND_PROPERTY_SET_H_
