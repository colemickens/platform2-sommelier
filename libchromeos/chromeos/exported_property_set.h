// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_EXPORTED_PROPERTY_SET_H_
#define LIBCHROMEOS_CHROMEOS_EXPORTED_PROPERTY_SET_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include <base/memory/weak_ptr.h>
#include <chromeos/any.h>
#include <chromeos/dbus_utils.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>

namespace chromeos {

namespace dbus_utils {

// This class may be used to implement the org.freedesktop.DBus.Properties
// interface.  It sends the update signal on property updates:
//
//   org.freedesktop.DBus.Properties.PropertiesChanged (
//       STRING interface_name,
//       DICT<STRING,VARIANT> changed_properties,
//       ARRAY<STRING> invalidated_properties);
//
//
// and implements the required methods of the interface:
//
//   org.freedesktop.DBus.Properties.Get(in STRING interface_name,
//                                       in STRING property_name,
//                                       out VARIANT value);
//   org.freedesktop.DBus.Properties.Set(in STRING interface_name,
//                                       in STRING property_name,
//                                       in VARIANT value);
//   org.freedesktop.DBus.Properties.GetAll(in STRING interface_name,
//                                          out DICT<STRING,VARIANT> props);
//
//  This class is very similar to the PropertySet class in Chrome, except that
//  it allows objects to expose properties rather than to consume them.
//  It is used as part of DBusObject to implement DBus object properties on
//  registered interfaces. See description of DBusObject class for more details.

class DBusObject;

class ExportedPropertyBase {
 public:
  ExportedPropertyBase() = default;
  virtual ~ExportedPropertyBase() = default;

  using OnUpdateCallback = base::Callback<void(const ExportedPropertyBase*)>;

  // Called by ExportedPropertySet to register a callback.  This callback
  // triggers ExportedPropertySet to send a signal from the properties
  // interface of the exported object.
  virtual void SetUpdateCallback(const OnUpdateCallback& cb);

  // Returns the contained value as Any.
  virtual chromeos::Any GetValue() const = 0;

 protected:
  // Notify the listeners of OnUpdateCallback that the property has changed.
  void NotifyPropertyChanged();

 private:
  OnUpdateCallback on_update_callback_;
};

class ExportedPropertySet {
 public:
  using PropertyWriter = base::Callback<void(dbus_utils::Dictionary* dict)>;

  explicit ExportedPropertySet(dbus::Bus* bus);
  virtual ~ExportedPropertySet() = default;

  // Called to notify ExportedPropertySet that the DBus object has been
  // exported successfully and property notification signals can be sent out.
  void OnObjectExported(dbus::ExportedObject* exported_object) {
    exported_object_ = exported_object;
  }

  // Return a callback that knows how to write this property set's properties
  // to a message.  This writer retains a weak pointer to this, and must
  // only be invoked on the same thread as the rest of ExportedPropertySet.
  PropertyWriter GetPropertyWriter(const std::string& interface_name);

  void RegisterProperty(const std::string& interface_name,
                        const std::string& property_name,
                        ExportedPropertyBase* exported_property);

  // DBus methods for org.freedesktop.DBus.Properties interface.
  dbus_utils::Dictionary HandleGetAll(
      chromeos::ErrorPtr* error,
      const std::string& interface_name);
  chromeos::Any HandleGet(
      chromeos::ErrorPtr* error,
      const std::string& interface_name,
      const std::string& property_name);
  // While Properties.Set has a handler to complete the interface,  we don't
  // support writable properties.  This is almost a feature, since bindings for
  // many languages don't support errors coming back from invalid writes.
  // Instead, use setters in exposed interfaces.
  void HandleSet(
      chromeos::ErrorPtr* error,
      const std::string& interface_name,
      const std::string& property_name,
      const chromeos::Any& value);
  // Returns a string-to-variant map of all the properties for the given
  // interface and their values.
  dbus_utils::Dictionary GetInterfaceProperties(
      const std::string& interface_name) const;

 private:
  // Used to write the dictionary of string->variant to a message.
  // This dictionary represents the property name/value pairs for the
  // given interface.
  void WritePropertiesToDict(const std::string& interface_name,
                             dbus_utils::Dictionary* dict);
  void HandlePropertyUpdated(const std::string& interface_name,
                             const std::string& property_name,
                             const ExportedPropertyBase* exported_property);

  dbus::Bus* bus_;  // weak; owned by outer DBusObject containing this object.
  dbus::ExportedObject* exported_object_ = nullptr;  // weak; owned by |bus_|.
  // This is a map from interface name -> property name -> pointer to property.
  std::map<std::string,
           std::map<std::string, ExportedPropertyBase*>> properties_;

  // D-Bus callbacks may last longer the property set exporting those methods.
  base::WeakPtrFactory<ExportedPropertySet> weak_ptr_factory_;

  friend class DBusObject;
  friend class ExportedPropertySetTest;
  DISALLOW_COPY_AND_ASSIGN(ExportedPropertySet);
};

template <typename T>
class ExportedProperty : public ExportedPropertyBase {
 public:
  ExportedProperty() = default;
  ~ExportedProperty() override = default;

  // Retrieves the current value.
  const T& value() const { return value_; }

  // Set the value exposed to remote applications.  This triggers notifications
  // of changes over the Properties interface.
  void SetValue(const T& new_value)  {
    if (value_ != new_value) {
      value_ = new_value;
      this->NotifyPropertyChanged();
    }
  }

  // Implementation provided by specialization.
  chromeos::Any GetValue() const override { return value_; }

 private:
  T value_{};

  DISALLOW_COPY_AND_ASSIGN(ExportedProperty);
};

}  // namespace dbus_utils

}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_EXPORTED_PROPERTY_SET_H_
