// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_DISPATCHER_PROPERTY_H_
#define BLUETOOTH_DISPATCHER_PROPERTY_H_

#include <map>
#include <memory>
#include <string>

#include <brillo/dbus/exported_property_set.h>
#include <dbus/property.h>

namespace bluetooth {

// Typeless property factory. This typeless class is needed to generalize many
// types of properties that share the same interface. Contains utilities to
// create properties and value copying.
class PropertyFactoryBase {
 public:
  PropertyFactoryBase() = default;
  virtual ~PropertyFactoryBase() = default;

  // Instantiates a dbus::Property having the same type as this factory.
  virtual std::unique_ptr<dbus::PropertyBase> CreateProperty() = 0;

  // Instantiates a brillo::dbus_utils::ExportedProperty having the same type
  // as this factory.
  virtual std::unique_ptr<brillo::dbus_utils::ExportedPropertyBase>
  CreateExportedProperty() = 0;

  // Copies the value from a dbus::Property to a
  // brillo::dbus_utils::ExportedProperty having the specified type.
  // Doesn't own the argument pointers and doesn't keep them either.
  virtual void CopyPropertyToExportedProperty(
      dbus::PropertyBase* property_base,
      brillo::dbus_utils::ExportedPropertyBase* exported_property_base) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PropertyFactoryBase);
};

// The type-specific property factory.
template <typename T>
class PropertyFactory : public PropertyFactoryBase {
 public:
  PropertyFactory() = default;
  ~PropertyFactory() override = default;

  std::unique_ptr<dbus::PropertyBase> CreateProperty() override {
    return std::make_unique<dbus::Property<T>>();
  }

  std::unique_ptr<brillo::dbus_utils::ExportedPropertyBase>
  CreateExportedProperty() override {
    return std::make_unique<brillo::dbus_utils::ExportedProperty<T>>();
  }

  void CopyPropertyToExportedProperty(dbus::PropertyBase* property_base,
                                      brillo::dbus_utils::ExportedPropertyBase*
                                          exported_property_base) override {
    dbus::Property<T>* property =
        static_cast<dbus::Property<T>*>(property_base);
    brillo::dbus_utils::ExportedProperty<T>* exported_property =
        static_cast<brillo::dbus_utils::ExportedProperty<T>*>(
            exported_property_base);

    // No need to copy the value if they are already the same. This is useful to
    // prevent unnecessary PropertiesChanged signal being emitted.
    if (property->value() == exported_property->value())
      return;
    exported_property->SetValue(property->value());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PropertyFactory);
};

// A dbus::PropertySet that also holds the individual properties.
class PropertySet : public dbus::PropertySet {
 public:
  using dbus::PropertySet::PropertySet;

  // Holds the specified property |property_base| and registers it with the
  // specified name |property_name|.
  void RegisterProperty(const std::string& property_name,
                        std::unique_ptr<dbus::PropertyBase> property_base);
  // Returns the previously registered property. This object owns the returned
  // pointer so callers should make sure that the returned pointer is not used
  // outside the lifespan of this object.
  dbus::PropertyBase* GetProperty(const std::string& property_name);

 private:
  // Keeps the registered properties.
  std::map<std::string, std::unique_ptr<dbus::PropertyBase>> properties_;

  DISALLOW_COPY_AND_ASSIGN(PropertySet);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_PROPERTY_H_
