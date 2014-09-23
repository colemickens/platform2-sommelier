// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/dbus/data_serialization.h>

#include <base/logging.h>
#include <chromeos/any.h>

namespace chromeos {
namespace dbus_utils {

bool AppendValueToWriter(dbus::MessageWriter* writer, bool value) {
  writer->AppendBool(value);
  return true;
}

bool AppendValueToWriter(dbus::MessageWriter* writer, uint8_t value) {
  writer->AppendByte(value);
  return true;
}

bool AppendValueToWriter(dbus::MessageWriter* writer, int16_t value) {
  writer->AppendInt16(value);
  return true;
}

bool AppendValueToWriter(dbus::MessageWriter* writer, uint16_t value) {
  writer->AppendUint16(value);
  return true;
}

bool AppendValueToWriter(dbus::MessageWriter* writer, int32_t value) {
  writer->AppendInt32(value);
  return true;
}

bool AppendValueToWriter(dbus::MessageWriter* writer, uint32_t value) {
  writer->AppendUint32(value);
  return true;
}

bool AppendValueToWriter(dbus::MessageWriter* writer, int64_t value) {
  writer->AppendInt64(value);
  return true;
}

bool AppendValueToWriter(dbus::MessageWriter* writer, uint64_t value) {
  writer->AppendUint64(value);
  return true;
}

bool AppendValueToWriter(dbus::MessageWriter* writer, double value) {
  writer->AppendDouble(value);
  return true;
}

bool AppendValueToWriter(dbus::MessageWriter* writer,
                         const std::string& value) {
  writer->AppendString(value);
  return true;
}

bool AppendValueToWriter(dbus::MessageWriter* writer, const char* value) {
  return AppendValueToWriter(writer, std::string(value));
}

bool AppendValueToWriter(dbus::MessageWriter* writer,
                         const dbus::ObjectPath& value) {
  writer->AppendObjectPath(value);
  return true;
}

bool AppendValueToWriter(dbus::MessageWriter* writer,
                         const dbus::FileDescriptor& value) {
  writer->AppendFileDescriptor(value);
  return true;
}

bool AppendValueToWriter(dbus::MessageWriter* writer,
                         const chromeos::Any& value) {
  return value.AppendToDBusMessageWriter(writer);
}

///////////////////////////////////////////////////////////////////////////////

bool PopValueFromReader(dbus::MessageReader* reader, bool* value) {
  dbus::MessageReader variant_reader(nullptr);
  return details::DescendIntoVariantIfPresent(&reader, &variant_reader) &&
         reader->PopBool(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, uint8_t* value) {
  dbus::MessageReader variant_reader(nullptr);
  return details::DescendIntoVariantIfPresent(&reader, &variant_reader) &&
         reader->PopByte(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, int16_t* value) {
  dbus::MessageReader variant_reader(nullptr);
  return details::DescendIntoVariantIfPresent(&reader, &variant_reader) &&
         reader->PopInt16(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, uint16_t* value) {
  dbus::MessageReader variant_reader(nullptr);
  return details::DescendIntoVariantIfPresent(&reader, &variant_reader) &&
         reader->PopUint16(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, int32_t* value) {
  dbus::MessageReader variant_reader(nullptr);
  return details::DescendIntoVariantIfPresent(&reader, &variant_reader) &&
         reader->PopInt32(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, uint32_t* value) {
  dbus::MessageReader variant_reader(nullptr);
  return details::DescendIntoVariantIfPresent(&reader, &variant_reader) &&
         reader->PopUint32(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, int64_t* value) {
  dbus::MessageReader variant_reader(nullptr);
  return details::DescendIntoVariantIfPresent(&reader, &variant_reader) &&
         reader->PopInt64(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, uint64_t* value) {
  dbus::MessageReader variant_reader(nullptr);
  return details::DescendIntoVariantIfPresent(&reader, &variant_reader) &&
         reader->PopUint64(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, double* value) {
  dbus::MessageReader variant_reader(nullptr);
  return details::DescendIntoVariantIfPresent(&reader, &variant_reader) &&
         reader->PopDouble(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, std::string* value) {
  dbus::MessageReader variant_reader(nullptr);
  return details::DescendIntoVariantIfPresent(&reader, &variant_reader) &&
         reader->PopString(value);
}

bool PopValueFromReader(dbus::MessageReader* reader, dbus::ObjectPath* value) {
  dbus::MessageReader variant_reader(nullptr);
  return details::DescendIntoVariantIfPresent(&reader, &variant_reader) &&
         reader->PopObjectPath(value);
}

bool PopValueFromReader(dbus::MessageReader* reader,
                        dbus::FileDescriptor* value) {
  dbus::MessageReader variant_reader(nullptr);
  bool ok = details::DescendIntoVariantIfPresent(&reader, &variant_reader) &&
            reader->PopFileDescriptor(value);
  if (ok)
    value->CheckValidity();
  return ok;
}

namespace {

// Helper methods for PopValueFromReader(dbus::MessageReader*, Any*)
// implementation. Pops a value of particular type from |reader| and assigns
// it to |value| of type Any.
template<typename T>
bool PopTypedValueFromReader(dbus::MessageReader* reader,
                             chromeos::Any* value) {
  T data{};
  if (!PopValueFromReader(reader, &data))
    return false;
  *value = std::move(data);
  return true;
}

// std::vector<T> overload.
template<typename T>
bool PopTypedArrayFromReader(dbus::MessageReader* reader,
                             chromeos::Any* value) {
  return PopTypedValueFromReader<std::vector<T>>(reader, value);
}

// std::map<KEY, VALUE> overload.
template<typename KEY, typename VALUE>
bool PopTypedMapFromReader(dbus::MessageReader* reader,
                           chromeos::Any* value) {
  return PopTypedValueFromReader<std::map<KEY, VALUE>>(reader, value);
}

// Helper methods for reading common ARRAY signatures into a Variant.
// Note that only common types are supported. If an additional specific
// type signature is required, feel free to add support for it.
bool PopArrayValueFromReader(dbus::MessageReader* reader,
                             chromeos::Any* value) {
  std::string signature = reader->GetDataSignature();
  if (signature == "ab")
    return PopTypedArrayFromReader<bool>(reader, value);
  else if (signature == "ay")
    return PopTypedArrayFromReader<uint8_t>(reader, value);
  else if (signature == "an")
    return PopTypedArrayFromReader<int16_t>(reader, value);
  else if (signature == "aq")
    return PopTypedArrayFromReader<uint16_t>(reader, value);
  else if (signature == "ai")
    return PopTypedArrayFromReader<int32_t>(reader, value);
  else if (signature == "au")
    return PopTypedArrayFromReader<uint32_t>(reader, value);
  else if (signature == "ax")
    return PopTypedArrayFromReader<int64_t>(reader, value);
  else if (signature == "at")
    return PopTypedArrayFromReader<uint64_t>(reader, value);
  else if (signature == "ad")
    return PopTypedArrayFromReader<double>(reader, value);
  else if (signature == "as")
    return PopTypedArrayFromReader<std::string>(reader, value);
  else if (signature == "ao")
    return PopTypedArrayFromReader<dbus::ObjectPath>(reader, value);
  else if (signature == "av")
    return PopTypedArrayFromReader<chromeos::Any>(reader, value);
  else if (signature == "a{ss}")
    return PopTypedMapFromReader<std::string, std::string>(reader, value);
  else if (signature == "a{sv}")
    return PopTypedMapFromReader<std::string, chromeos::Any>(reader, value);
  else if (signature == "a{sa{ss}}")
    return PopTypedMapFromReader<
        std::string, std::map<std::string, std::string>>(reader, value);
  else if (signature == "a{sa{sv}}")
    return PopTypedMapFromReader<
        std::string, std::map<std::string, chromeos::Any>>(reader, value);

  // When a use case for particular array signature is found, feel free
  // to add handing for it here.
  LOG(ERROR) << "Variant de-serialization of array containing data of "
              << "type '" << signature << "' is not yet supported";
  return false;
}

// Helper methods for reading common STRUCT signatures into a Variant.
// Note that only common types are supported. If an additional specific
// type signature is required, feel free to add support for it.
bool PopStructValueFromReader(dbus::MessageReader* reader,
                              chromeos::Any* value) {
  std::string signature = reader->GetDataSignature();
  if (signature == "(ii)")
    return PopTypedValueFromReader<std::pair<int, int>>(reader, value);
  else if (signature == "(ss)")
    return PopTypedValueFromReader<std::pair<std::string, std::string>>(reader,
                                                                        value);

  // When a use case for particular struct signature is found, feel free
  // to add handing for it here.
  LOG(ERROR) << "Variant de-serialization of structs of type '"
              << signature << "' is not yet supported";
  return false;
}

}  // anonymous namespace

bool PopValueFromReader(dbus::MessageReader* reader, chromeos::Any* value) {
  if (reader->GetDataType() != dbus::Message::VARIANT)
    return false;
  dbus::MessageReader variant_reader(nullptr);
  if (!reader->PopVariant(&variant_reader))
    return false;

  switch (variant_reader.GetDataType()) {
    case dbus::Message::BYTE:
      return PopTypedValueFromReader<uint8_t>(&variant_reader, value);
    case dbus::Message::BOOL:
      return PopTypedValueFromReader<bool>(&variant_reader, value);
    case dbus::Message::INT16:
      return PopTypedValueFromReader<int16_t>(&variant_reader, value);
    case dbus::Message::UINT16:
      return PopTypedValueFromReader<uint16_t>(&variant_reader, value);
    case dbus::Message::INT32:
      return PopTypedValueFromReader<int32_t>(&variant_reader, value);
    case dbus::Message::UINT32:
      return PopTypedValueFromReader<uint32_t>(&variant_reader, value);
    case dbus::Message::INT64:
      return PopTypedValueFromReader<int64_t>(&variant_reader, value);
    case dbus::Message::UINT64:
      return PopTypedValueFromReader<uint64_t>(&variant_reader, value);
    case dbus::Message::DOUBLE:
      return PopTypedValueFromReader<double>(&variant_reader, value);
    case dbus::Message::STRING:
      return PopTypedValueFromReader<std::string>(&variant_reader, value);
    case dbus::Message::OBJECT_PATH:
      return PopTypedValueFromReader<dbus::ObjectPath>(&variant_reader, value);
    case dbus::Message::ARRAY:
      return PopArrayValueFromReader(&variant_reader, value);
    case dbus::Message::STRUCT:
      return PopStructValueFromReader(&variant_reader, value);
    case dbus::Message::DICT_ENTRY:
      LOG(ERROR) << "Variant of DICT_ENTRY is invalid";
      return false;
    case dbus::Message::VARIANT:
      LOG(ERROR) << "Variant containing a variant is invalid";
      return false;
    case dbus::Message::UNIX_FD:
      CHECK(dbus::IsDBusTypeUnixFdSupported()) << "UNIX_FD data not supported";
      // dbus::FileDescriptor is not a copyable type. Cannot be returned via
      // chromeos::Any. Fail here.
      LOG(ERROR) << "Cannot return FileDescriptor via Any";
      return false;
    default:
      LOG(FATAL) << "Unknown D-Bus data type: " << variant_reader.GetDataType();
      return false;
  }
  return true;
}

}  // namespace dbus_utils
}  // namespace chromeos
