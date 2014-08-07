// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/exported_property_set.h"

#include <base/bind.h>
#include <dbus/bus.h>  // For kPropertyInterface
#include <dbus/property.h>  // For kPropertyInterface

#include "chromeos/async_event_sequencer.h"
#include "chromeos/dbus_utils.h"

using chromeos::dbus_utils::AsyncEventSequencer;

namespace chromeos {

namespace dbus_utils {

namespace {
const char kExportFailedMessage[] = "Failed to register DBus method.";
}  // namespace

ExportedPropertySet::ExportedPropertySet(dbus::Bus* bus,
                                         const dbus::ObjectPath& path)
    : bus_(bus), exported_object_(bus->GetExportedObject(path)),
      weak_ptr_factory_(this) { }

void ExportedPropertySet::Init(const OnInitFinish& cb) {
  bus_->AssertOnOriginThread();
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  exported_object_->ExportMethod(
      dbus::kPropertiesInterface, dbus::kPropertiesGetAll,
      base::Bind(&ExportedPropertySet::HandleGetAll,
                 weak_ptr_factory_.GetWeakPtr()),
      sequencer->GetExportHandler(
          dbus::kPropertiesInterface, dbus::kPropertiesGetAll,
          kExportFailedMessage, false));
  exported_object_->ExportMethod(
      dbus::kPropertiesInterface, dbus::kPropertiesGet,
      base::Bind(&ExportedPropertySet::HandleGet,
                 weak_ptr_factory_.GetWeakPtr()),
      sequencer->GetExportHandler(
          dbus::kPropertiesInterface, dbus::kPropertiesGet,
          kExportFailedMessage, false));
  exported_object_->ExportMethod(
      dbus::kPropertiesInterface, dbus::kPropertiesSet,
      base::Bind(&ExportedPropertySet::HandleSet,
                 weak_ptr_factory_.GetWeakPtr()),
      sequencer->GetExportHandler(
          dbus::kPropertiesInterface, dbus::kPropertiesSet,
          kExportFailedMessage, false));
  sequencer->OnAllTasksCompletedCall({cb});
}

ExportedPropertySet::PropertyWriter ExportedPropertySet::GetPropertyWriter(
    const std::string& interface) {
  return base::Bind(&ExportedPropertySet::WritePropertiesDictToMessage,
                    weak_ptr_factory_.GetWeakPtr(),
                    interface);
}

void ExportedPropertySet::RegisterProperty(
    const std::string& interface_name,
    const std::string& property_name,
    ExportedPropertyBase* exported_property) {
  bus_->AssertOnOriginThread();
  properties_[interface_name][property_name] = exported_property;
  // Technically, the property set exists longer than the properties themselves,
  // so we could use Unretained here rather than a weak pointer.
  ExportedPropertyBase::OnUpdateCallback cb = base::Bind(
      &ExportedPropertySet::HandlePropertyUpdated,
      weak_ptr_factory_.GetWeakPtr(),
      interface_name, property_name);
  exported_property->SetUpdateCallback(cb);
}

void ExportedPropertySet::HandleGetAll(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  bus_->AssertOnOriginThread();
  dbus::MessageReader reader(method_call);
  std::string interface_name;
  if (!reader.PopString(&interface_name)) {
    response_sender.Run(
        GetBadArgsError(method_call, "No interface name specified."));
    return;
  }
  if (reader.HasMoreData()) {
    response_sender.Run(
        GetBadArgsError(method_call, "Too many arguments to GetAll."));
    return;
  }
  scoped_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(method_call));
  dbus::MessageWriter resp_writer(response.get());
  WritePropertiesDictToMessage(interface_name, &resp_writer);
  response_sender.Run(response.Pass());
}

void ExportedPropertySet::WritePropertiesDictToMessage(
    const std::string& interface_name,
    dbus::MessageWriter* writer) {
  dbus::MessageWriter dict_writer(nullptr);
  writer->OpenArray("{sv}", &dict_writer);
  auto property_map_itr = properties_.find(interface_name);
  if (property_map_itr != properties_.end()) {
    for (const auto& kv : property_map_itr->second) {
      dbus::MessageWriter entry_writer(nullptr);
      dict_writer.OpenDictEntry(&entry_writer);
      entry_writer.AppendString(kv.first);
      kv.second->AppendValueToWriter(&entry_writer);
      dict_writer.CloseContainer(&entry_writer);
    }
  } else {
    LOG(WARNING) << "No properties found for interface interface_name";
  }
  writer->CloseContainer(&dict_writer);
}

void ExportedPropertySet::HandleGet(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  bus_->AssertOnOriginThread();
  dbus::MessageReader reader(method_call);
  std::string interface_name, property_name;
  if (!reader.PopString(&interface_name)) {
    response_sender.Run(
        GetBadArgsError(method_call, "No interface name specified."));
    return;
  }
  if (!reader.PopString(&property_name)) {
    response_sender.Run(
        GetBadArgsError(method_call, "No property name specified."));
    return;
  }
  if (reader.HasMoreData()) {
    response_sender.Run(
        GetBadArgsError(method_call, "Too many arguments to Get."));
    return;
  }
  auto property_map_itr = properties_.find(interface_name);
  if (property_map_itr == properties_.end()) {
    response_sender.Run(
        GetBadArgsError(method_call, "No such interface on object."));
    return;
  }
  LOG(ERROR) << "Looking for " << property_name << " on " << interface_name;
  auto property_itr = property_map_itr->second.find(property_name);
  if (property_itr == property_map_itr->second.end()) {
    response_sender.Run(
        GetBadArgsError(method_call, "No such property on interface."));
    return;
  }
  scoped_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(method_call));
  dbus::MessageWriter resp_writer(response.get());
  property_itr->second->AppendValueToWriter(&resp_writer);
  response_sender.Run(response.Pass());
}

void ExportedPropertySet::HandleSet(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  bus_->AssertOnOriginThread();
  scoped_ptr<dbus::ErrorResponse> error_resp(
      dbus::ErrorResponse::FromMethodCall(
          method_call, "org.freedesktop.DBus.Error.NotSupported", ""));
  scoped_ptr<dbus::Response> response(error_resp.release());
  response_sender.Run(response.Pass());
}

void ExportedPropertySet::HandlePropertyUpdated(
    const std::string& interface,
    const std::string& name,
    const ExportedPropertyBase* property) {
  bus_->AssertOnOriginThread();
  dbus::Signal signal(dbus::kPropertiesInterface, dbus::kPropertiesChanged);
  dbus::MessageWriter writer(&signal);
  dbus::MessageWriter array_writer(nullptr);
  dbus::MessageWriter dict_writer(nullptr);
  writer.AppendString(interface);
  writer.OpenArray("{sv}", &array_writer);
  array_writer.OpenDictEntry(&dict_writer);
  dict_writer.AppendString(name);
  property->AppendValueToWriter(&dict_writer);
  array_writer.CloseContainer(&dict_writer);
  writer.CloseContainer(&array_writer);
  // The interface specification tells us to include this list of properties
  // which have changed, but for whom no value is conveyed.  Currently, we
  // don't do anything interesting here.
  writer.OpenArray("s", &array_writer);
  writer.CloseContainer(&array_writer);
  // This sends the signal asyncronously.  However, the raw message inside
  // the signal object is ref-counted, so we're fine to allocate the Signal
  // object on our local stack.
  exported_object_->SendSignal(&signal);
}

template <typename T>
void AppendPropertyToWriter(dbus::MessageWriter* writer, const T& value);

template <>
void AppendPropertyToWriter(dbus::MessageWriter* writer, const bool& value) {
  writer->AppendVariantOfBool(value);
}

template <>
void AppendPropertyToWriter(dbus::MessageWriter* writer, const uint8& value) {
  writer->AppendVariantOfByte(value);
}

template <>
void AppendPropertyToWriter(dbus::MessageWriter* writer, const int16& value) {
  writer->AppendVariantOfInt16(value);
}

template <>
void AppendPropertyToWriter(dbus::MessageWriter* writer, const uint16& value) {
  writer->AppendVariantOfUint16(value);
}

template <>
void AppendPropertyToWriter(dbus::MessageWriter* writer, const int32& value) {
  writer->AppendVariantOfInt32(value);
}

template <>
void AppendPropertyToWriter(dbus::MessageWriter* writer, const uint32& value) {
  writer->AppendVariantOfUint32(value);
}

template <>
void AppendPropertyToWriter(dbus::MessageWriter* writer, const int64& value) {
  writer->AppendVariantOfInt64(value);
}

template <>
void AppendPropertyToWriter(dbus::MessageWriter* writer, const uint64& value) {
  writer->AppendVariantOfUint64(value);
}

template <>
void AppendPropertyToWriter(dbus::MessageWriter* writer, const double& value) {
  writer->AppendVariantOfDouble(value);
}

template <>
void AppendPropertyToWriter(
    dbus::MessageWriter* writer, const std::string& value) {
  writer->AppendVariantOfString(value);
}

template <>
void AppendPropertyToWriter(
    dbus::MessageWriter* writer, const dbus::ObjectPath& value) {
  writer->AppendVariantOfObjectPath(value);
}

template <>
void AppendPropertyToWriter(
    dbus::MessageWriter* writer, const std::vector<std::string>& value) {
  dbus::MessageWriter variant_writer(nullptr);
  writer->OpenVariant("as", &variant_writer);
  variant_writer.AppendArrayOfStrings(value);
  writer->CloseContainer(&variant_writer);
}

template <>
void AppendPropertyToWriter(
    dbus::MessageWriter* writer, const std::vector<dbus::ObjectPath>& value) {
  dbus::MessageWriter variant_writer(nullptr);
  writer->OpenVariant("ao", &variant_writer);
  variant_writer.AppendArrayOfObjectPaths(value);
  writer->CloseContainer(&variant_writer);
}

template <>
void AppendPropertyToWriter(
    dbus::MessageWriter* writer, const std::vector<uint8>& value) {
  dbus::MessageWriter variant_writer(nullptr);
  writer->OpenVariant("ay", &variant_writer);
  variant_writer.AppendArrayOfBytes(value.data(), value.size());
  writer->CloseContainer(&variant_writer);
}

template <typename T>
ExportedProperty<T>::ExportedProperty() {}

template <typename T>
ExportedProperty<T>::~ExportedProperty() {}

template <typename T>
const T& ExportedProperty<T>::value() const { return value_; }

template <typename T>
void ExportedProperty<T>::SetValue(const T& new_value) {
  if (value_ == new_value) {
    return;
  }
  value_ = new_value;
  // These is a brief period after the construction of an ExportedProperty
  // when this callback is not initialized because the property has not
  // been registered with the parent ExportedPropertySet.  During this period
  // users should be initializing values via SetValue, and no notifications
  // should be triggered by the ExportedPropertySet.
  if (!on_update_.is_null()) {
    on_update_.Run(this);
  }
}

template <typename T>
void ExportedProperty<T>::SetUpdateCallback(const OnUpdateCallback& cb) {
  on_update_ = cb;
}

template <typename T>
void ExportedProperty<T>::AppendValueToWriter(
    dbus::MessageWriter* writer) const {
  AppendPropertyToWriter(writer, value_);
}

template class ExportedProperty<bool>;
template class ExportedProperty<uint8>;
template class ExportedProperty<int16>;
template class ExportedProperty<uint16>;
template class ExportedProperty<int32>;
template class ExportedProperty<uint32>;
template class ExportedProperty<int64>;
template class ExportedProperty<uint64>;
template class ExportedProperty<double>;
template class ExportedProperty<std::string>;
template class ExportedProperty<dbus::ObjectPath>;
template class ExportedProperty<std::vector<std::string>>;
template class ExportedProperty<std::vector<dbus::ObjectPath>>;
template class ExportedProperty<std::vector<uint8>>;

}  // namespace dbus_utils

}  // namespace chromeos
