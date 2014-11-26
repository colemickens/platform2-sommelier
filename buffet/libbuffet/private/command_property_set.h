// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_LIBBUFFET_PRIVATE_COMMAND_PROPERTY_SET_H_
#define BUFFET_LIBBUFFET_PRIVATE_COMMAND_PROPERTY_SET_H_

#include <string>

#include <base/macros.h>
#include <chromeos/dbus/dbus_property.h>
#include <chromeos/variant_dictionary.h>
#include <dbus/object_manager.h>

#include "buffet/libbuffet/export.h"

namespace buffet {

// PropertySet for remote D-Bus GCD Command object from Buffet daemon.
class CommandPropertySet : public dbus::PropertySet {
 public:
  CommandPropertySet(dbus::ObjectProxy* object_proxy,
                     const std::string& interface_name,
                     const PropertyChangedCallback& callback);
  chromeos::dbus_utils::Property<std::string> id;
  chromeos::dbus_utils::Property<std::string> name;
  chromeos::dbus_utils::Property<std::string> category;
  chromeos::dbus_utils::Property<std::string> status;
  chromeos::dbus_utils::Property<int32_t> progress;
  chromeos::dbus_utils::Property<chromeos::VariantDictionary> parameters;
  chromeos::dbus_utils::Property<chromeos::VariantDictionary> results;

 private:
  DISALLOW_COPY_AND_ASSIGN(CommandPropertySet);
};

}  // namespace buffet

#endif  // BUFFET_LIBBUFFET_PRIVATE_COMMAND_PROPERTY_SET_H_
