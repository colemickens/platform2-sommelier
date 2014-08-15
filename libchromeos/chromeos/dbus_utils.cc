// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus_utils.h"

#include <base/bind.h>
#include <base/logging.h>
#include <base/values.h>
#include <dbus/values_util.h>

namespace chromeos {

namespace dbus_utils {

namespace {

// Passes |method_call| to |handler| and passes the response to
// |response_sender|. If |handler| returns NULL, an empty response is created
// and sent.
void HandleSynchronousDBusMethodCall(
    base::Callback<scoped_ptr<dbus::Response>(dbus::MethodCall*)> handler,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  auto response = handler.Run(method_call);
  if (!response)
    response = dbus::Response::FromMethodCall(method_call);

  response_sender.Run(response.Pass());
}

}  // namespace


scoped_ptr<dbus::Response> GetBadArgsError(dbus::MethodCall* method_call,
                                           const std::string& message) {
  LOG(ERROR) << "Error while handling DBus call: " << message;
  scoped_ptr<dbus::ErrorResponse> resp(dbus::ErrorResponse::FromMethodCall(
      method_call, "org.freedesktop.DBus.Error.InvalidArgs", message));
  return scoped_ptr<dbus::Response>(resp.release());
}

std::unique_ptr<dbus::Response> GetDBusError(dbus::MethodCall* method_call,
                                             const chromeos::Error* error) {
  std::string message;
  while (error) {
    // Format error string as "domain/code:message".
    if (!message.empty())
      message += ';';
    message += error->GetDomain() + '/' + error->GetCode() + ':' +
               error->GetMessage();
    error = error->GetInnerError();
  }
  scoped_ptr<dbus::ErrorResponse> resp(dbus::ErrorResponse::FromMethodCall(
    method_call, "org.freedesktop.DBus.Error.Failed", message));
  return std::unique_ptr<dbus::Response>(resp.release());
}

dbus::ExportedObject::MethodCallCallback GetExportableDBusMethod(
    base::Callback<scoped_ptr<dbus::Response>(dbus::MethodCall*)> handler) {
  return base::Bind(&HandleSynchronousDBusMethodCall, handler);
}

void AppendValueToWriter(dbus::MessageWriter* writer, bool value) {
  writer->AppendBool(value);
}

void AppendValueToWriter(dbus::MessageWriter* writer, uint8_t value) {
  writer->AppendByte(value);
}

void AppendValueToWriter(dbus::MessageWriter* writer, int16_t value) {
  writer->AppendInt16(value);
}

void AppendValueToWriter(dbus::MessageWriter* writer, uint16_t value) {
  writer->AppendUint16(value);
}

void AppendValueToWriter(dbus::MessageWriter* writer, int32_t value) {
  writer->AppendInt32(value);
}

void AppendValueToWriter(dbus::MessageWriter* writer, uint32_t value) {
  writer->AppendUint32(value);
}

void AppendValueToWriter(dbus::MessageWriter* writer, int64_t value) {
  writer->AppendInt64(value);
}

void AppendValueToWriter(dbus::MessageWriter* writer, uint64_t value) {
  writer->AppendUint64(value);
}

void AppendValueToWriter(dbus::MessageWriter* writer, double value) {
  writer->AppendDouble(value);
}

void AppendValueToWriter(dbus::MessageWriter* writer,
                         const std::string& value) {
  writer->AppendString(value);
}

void AppendValueToWriter(dbus::MessageWriter* writer,
                         const dbus::ObjectPath& value) {
  writer->AppendObjectPath(value);
}

void AppendValueToWriter(dbus::MessageWriter* writer,
                         const std::vector<std::string>& value) {
  writer->AppendArrayOfStrings(value);
}

void AppendValueToWriter(dbus::MessageWriter* writer,
                         const std::vector<dbus::ObjectPath>& value) {
  writer->AppendArrayOfObjectPaths(value);
}

void AppendValueToWriter(dbus::MessageWriter* writer,
                         const std::vector<uint8_t>& value) {
  writer->AppendArrayOfBytes(value.data(), value.size());
}

void AppendValueToWriter(dbus::MessageWriter* writer, const Dictionary& value) {
  dbus::MessageWriter dict_writer(nullptr);
  writer->OpenArray("{sv}", &dict_writer);
  for (const auto& pair : value) {
    dbus::MessageWriter entry_writer(nullptr);
    dict_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(pair.first);
    dbus::AppendBasicTypeValueDataAsVariant(&entry_writer, *pair.second);
    dict_writer.CloseContainer(&entry_writer);
  }
  writer->CloseContainer(&dict_writer);
}

///////////////////////////////////////////////////////////////////////////////

void AppendValueToWriterAsVariant(dbus::MessageWriter* writer, bool value) {
  writer->AppendVariantOfBool(value);
}

void AppendValueToWriterAsVariant(dbus::MessageWriter* writer, uint8_t value) {
  writer->AppendVariantOfByte(value);
}

void AppendValueToWriterAsVariant(dbus::MessageWriter* writer, int16_t value) {
  writer->AppendVariantOfInt16(value);
}

void AppendValueToWriterAsVariant(dbus::MessageWriter* writer, uint16_t value) {
  writer->AppendVariantOfUint16(value);
}

void AppendValueToWriterAsVariant(dbus::MessageWriter* writer, int32_t value) {
  writer->AppendVariantOfInt32(value);
}

void AppendValueToWriterAsVariant(dbus::MessageWriter* writer, uint32_t value) {
  writer->AppendVariantOfUint32(value);
}

void AppendValueToWriterAsVariant(dbus::MessageWriter* writer, int64_t value) {
  writer->AppendVariantOfInt64(value);
}

void AppendValueToWriterAsVariant(dbus::MessageWriter* writer, uint64_t value) {
  writer->AppendVariantOfUint64(value);
}

void AppendValueToWriterAsVariant(dbus::MessageWriter* writer, double value) {
  writer->AppendVariantOfDouble(value);
}

void AppendValueToWriterAsVariant(dbus::MessageWriter* writer,
                                  const std::string& value) {
  writer->AppendVariantOfString(value);
}

void AppendValueToWriterAsVariant(dbus::MessageWriter* writer,
                                  const dbus::ObjectPath& value) {
  writer->AppendVariantOfObjectPath(value);
}

void AppendValueToWriterAsVariant(dbus::MessageWriter* writer,
                                  const std::vector<std::string>& value) {
  dbus::MessageWriter variant_writer(nullptr);
  writer->OpenVariant("as", &variant_writer);
  variant_writer.AppendArrayOfStrings(value);
  writer->CloseContainer(&variant_writer);
}

void AppendValueToWriterAsVariant(dbus::MessageWriter* writer,
                                  const std::vector<dbus::ObjectPath>& value) {
  dbus::MessageWriter variant_writer(nullptr);
  writer->OpenVariant("ao", &variant_writer);
  variant_writer.AppendArrayOfObjectPaths(value);
  writer->CloseContainer(&variant_writer);
}

void AppendValueToWriterAsVariant(dbus::MessageWriter* writer,
                                  const std::vector<uint8_t>& value) {
  dbus::MessageWriter variant_writer(nullptr);
  writer->OpenVariant("ay", &variant_writer);
  variant_writer.AppendArrayOfBytes(value.data(), value.size());
  writer->CloseContainer(&variant_writer);
}

void AppendValueToWriterAsVariant(dbus::MessageWriter* writer,
                                  const Dictionary& value) {
  dbus::MessageWriter variant_writer(nullptr);
  writer->OpenVariant("a{sv}", &variant_writer);
  AppendValueToWriter(&variant_writer, value);
  writer->CloseContainer(&variant_writer);
}

///////////////////////////////////////////////////////////////////////////////

bool PopValueFromReader(dbus::MessageReader* reader, bool* value) {
  return (reader->GetDataType() == dbus::Message::VARIANT) ?
    reader->PopVariantOfBool(value) : reader->PopBool(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, uint8_t* value) {
  return (reader->GetDataType() == dbus::Message::VARIANT) ?
    reader->PopVariantOfByte(value) : reader->PopByte(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, int16_t* value) {
  return (reader->GetDataType() == dbus::Message::VARIANT) ?
    reader->PopVariantOfInt16(value) : reader->PopInt16(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, uint16_t* value) {
  return (reader->GetDataType() == dbus::Message::VARIANT) ?
    reader->PopVariantOfUint16(value) : reader->PopUint16(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, int32_t* value) {
  return (reader->GetDataType() == dbus::Message::VARIANT) ?
    reader->PopVariantOfInt32(value) : reader->PopInt32(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, uint32_t* value) {
  return (reader->GetDataType() == dbus::Message::VARIANT) ?
    reader->PopVariantOfUint32(value) : reader->PopUint32(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, int64_t* value) {
  return (reader->GetDataType() == dbus::Message::VARIANT) ?
    reader->PopVariantOfInt64(value) : reader->PopInt64(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, uint64_t* value) {
  return (reader->GetDataType() == dbus::Message::VARIANT) ?
    reader->PopVariantOfUint64(value) : reader->PopUint64(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, double* value) {
  return (reader->GetDataType() == dbus::Message::VARIANT) ?
    reader->PopVariantOfDouble(value) : reader->PopDouble(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, std::string* value) {
  return (reader->GetDataType() == dbus::Message::VARIANT) ?
    reader->PopVariantOfString(value) : reader->PopString(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, dbus::ObjectPath* value) {
  return (reader->GetDataType() == dbus::Message::VARIANT) ?
    reader->PopVariantOfObjectPath(value) : reader->PopObjectPath(value);
}

bool PopValueFromReader(dbus::MessageReader* reader,
                        std::vector<std::string>* value) {
  dbus::MessageReader variant_reader(nullptr);
  if (reader->GetDataType() == dbus::Message::VARIANT) {
    if (!reader->PopVariant(&variant_reader))
      return false;
    reader = &variant_reader;
  }
  return reader->PopArrayOfStrings(value);
}

bool PopValueFromReader(dbus::MessageReader* reader,
                        std::vector<dbus::ObjectPath>* value) {
  dbus::MessageReader variant_reader(nullptr);
  if (reader->GetDataType() == dbus::Message::VARIANT) {
    if (!reader->PopVariant(&variant_reader))
      return false;
    reader = &variant_reader;
  }
  return reader->PopArrayOfObjectPaths(value);
}

bool PopValueFromReader(dbus::MessageReader* reader,
                        std::vector<uint8_t>* value) {
  dbus::MessageReader variant_reader(nullptr);
  if (reader->GetDataType() == dbus::Message::VARIANT) {
    if (!reader->PopVariant(&variant_reader))
      return false;
    reader = &variant_reader;
  }
  const uint8_t* data_ptr = nullptr;
  size_t data_len = 0;
  if (!reader->PopArrayOfBytes(&data_ptr, &data_len))
    return false;
  value->assign(data_ptr, data_ptr + data_len);
  return true;
}

bool PopValueFromReader(dbus::MessageReader* reader, Dictionary* value) {
  dbus::MessageReader variant_reader(nullptr);
  if (reader->GetDataType() == dbus::Message::VARIANT) {
    if (!reader->PopVariant(&variant_reader))
      return false;
    reader = &variant_reader;
  }
  dbus::MessageReader array_reader(nullptr);
  if (!reader->PopArray(&array_reader))
    return false;
  while (array_reader.HasMoreData()) {
    dbus::MessageReader dict_entry_reader(nullptr);
    if (!array_reader.PopDictEntry(&dict_entry_reader))
      return false;
    std::string key;
    if (!dict_entry_reader.PopString(&key))
      return false;
    base::Value* data = dbus::PopDataAsValue(&dict_entry_reader);
    if (!data)
      return false;
    value->insert(std::make_pair(key, std::unique_ptr<base::Value>(data)));
  }
  return true;
}

}  // namespace dbus_utils

}  // namespace chromeos
