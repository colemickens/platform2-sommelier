// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_DBUS_DATA_SERIALIZATION_H_
#define LIBCHROMEOS_CHROMEOS_DBUS_DATA_SERIALIZATION_H_

// The main functionality provided by this header file is methods to serialize
// native C++ data over D-Bus. This includes three major parts:
// - Methods to get the D-Bus signature for a given C++ type:
//     std::string GetDBusSignature<T>();
// - Methods to write arbitrary C++ data to D-Bus MessageWriter:
//     bool AppendValueToWriter(dbus::MessageWriter* writer, const T& value);
//     bool AppendValueToWriterAsVariant(dbus::MessageWriter*, const T&);
// - Methods to read arbitrary C++ data from D-Bus MessageReader:
//     bool PopValueFromReader(dbus::MessageReader* reader, T* value);
//     bool PopVariantValueFromReader(dbus::MessageReader* reader, T* value);
//
// There are a number of overloads to handle C++ equivalents of basic D-Bus
// types:
//   D-Bus Type  | D-Bus Signature | Native C++ type
//  --------------------------------------------------
//   BYTE        |        y        |  uint8_t
//   BOOL        |        b        |  bool
//   INT16       |        n        |  int16_t
//   UINT16      |        q        |  uint16_t
//   INT32       |        i        |  int32_t (int)
//   UINT32      |        u        |  uint32_t (unsigned)
//   INT64       |        x        |  int64_t
//   UINT64      |        t        |  uint64_t
//   DOUBLE      |        d        |  double
//   STRING      |        s        |  std::string
//   OBJECT_PATH |        o        |  dbus::ObjectPath
//   ARRAY       |        aT       |  std::vector<T>
//   STRUCT      |       (UV)      |  std::pair<U,V> (*)
//   DICT        |       a{KV}     |  std::map<K,V>
//   VARIANT     |        v        |  chromeos::Any
//   UNIX_FD     |        h        |  dbus::FileDescriptor
//   SIGNATURE   |        g        |  (unsupported)
//
// (*) - Currently, only 2 element STRUCTs are supported as std::pair. In the
// future we can add a generic std::tuple<...> support for arbitrary number
// of struct members. Implementations could also provide specializations for
// custom C++ structures for AppendValueToWriter/PopValueFromReader.
// See an example in DBusUtils.CustomStruct unit test in dbus_utils_unittest.cc.

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <chromeos/chromeos_export.h>
#include <base/logging.h>
#include <dbus/message.h>

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace chromeos {

// Forward-declare only. Can't include any.h right now because it needs
// AppendValueToWriter() declared below.
class Any;

namespace dbus_utils {

using Dictionary = std::map<std::string, chromeos::Any>;

//----------------------------------------------------------------------------
// Get D-Bus data signature from C++ data types.
// Specializations of a generic GetDBusSignature<T>() provide signature strings
// for native C++ types.

// It is not possible to provide partial specialization for template functions,
// so we are using a helper template struct with a static member getter,
// DBusSignature<T>::get(), to work around this problem. So, we do partial
// specialization of DBusSignature<T> instead, and have GetDBusSignature<T>()
// just call DBusSignature<T>::get().

template<typename T> std::string GetDBusSignature();  // Forward declaration.

// Generic template getter that fails for all types unless an explicit
// specialization is provided for.
template<typename T>
struct DBusSignature {
  inline static std::string get() {
    LOG(ERROR) << "Type '" << typeid(T).name() << "' is not supported by D-Bus";
    return std::string();
  }
};

// Helper method to format the type string of an array.
// Essentially it adds "a" in front of |element_signature|.
inline std::string GetArrayDBusSignature(const std::string& element_signature) {
  return DBUS_TYPE_ARRAY_AS_STRING + element_signature;
}

// Helper method to get a signature string for DICT_ENTRY.
// Returns "{KV}", where "K" and "V" are the type signatures for types
// KEY/VALUE. For example, GetDBusDictEntryType<std::string, int>() would return
// "{si}".
template<typename KEY, typename VALUE>
inline std::string GetDBusDictEntryType() {
  return DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING +
         GetDBusSignature<KEY>() + GetDBusSignature<VALUE>() +
         DBUS_DICT_ENTRY_END_CHAR_AS_STRING;
}

// Specializations of DBusSignature<T> for basic D-Bus types.
template<> struct DBusSignature<bool> {
  inline static std::string get() { return DBUS_TYPE_BOOLEAN_AS_STRING; }
};
template<> struct DBusSignature<uint8_t> {
  inline static std::string get() { return DBUS_TYPE_BYTE_AS_STRING; }
};
template<> struct DBusSignature<int16_t> {
  inline static std::string get() { return DBUS_TYPE_INT16_AS_STRING; }
};
template<> struct DBusSignature<uint16_t> {
  inline static std::string get() { return DBUS_TYPE_UINT16_AS_STRING; }
};
template<> struct DBusSignature<int32_t> {
  inline static std::string get() { return DBUS_TYPE_INT32_AS_STRING; }
};
template<> struct DBusSignature<uint32_t> {
  inline static std::string get() { return DBUS_TYPE_UINT32_AS_STRING; }
};
template<> struct DBusSignature<int64_t> {
  inline static std::string get() { return DBUS_TYPE_INT64_AS_STRING; }
};
template<> struct DBusSignature<uint64_t> {
  inline static std::string get() { return DBUS_TYPE_UINT64_AS_STRING; }
};
template<> struct DBusSignature<double> {
  inline static std::string get() { return DBUS_TYPE_DOUBLE_AS_STRING; }
};
template<> struct DBusSignature<const char*> {
  inline static std::string get() { return DBUS_TYPE_STRING_AS_STRING; }
};
template<> struct DBusSignature<const char[]> {
  inline static std::string get() { return DBUS_TYPE_STRING_AS_STRING; }
};
template<> struct DBusSignature<std::string> {
  inline static std::string get() { return DBUS_TYPE_STRING_AS_STRING; }
};
template<> struct DBusSignature<dbus::ObjectPath> {
  inline static std::string get() { return DBUS_TYPE_OBJECT_PATH_AS_STRING; }
};
template<> struct DBusSignature<dbus::FileDescriptor> {
  inline static std::string get() { return DBUS_TYPE_UNIX_FD_AS_STRING; }
};
template<> struct DBusSignature<chromeos::Any> {
  inline static std::string get() { return DBUS_TYPE_VARIANT_AS_STRING; }
};

// Specializations of DBusSignature<T> for some compound data types such
// as ARRAYs, STRUCTs, DICTs.

// std::vector = D-Bus ARRAY.
template<typename T>
struct DBusSignature<std::vector<T>> {
  // Returns "aT", where "T" is the signature string for type T.
  inline static std::string get() {
    return GetArrayDBusSignature(GetDBusSignature<T>());
  }
};

// std::pair = D-Bus STRUCT with two elements.
template<typename U, typename V>
struct DBusSignature<std::pair<U, V>> {
  // Returns "(UV)", where "U" and "V" are the signature strings for types U/V.
  inline static std::string get() {
    return DBUS_STRUCT_BEGIN_CHAR_AS_STRING + GetDBusSignature<U>() +
           GetDBusSignature<V>() + DBUS_STRUCT_END_CHAR_AS_STRING;
  }
};

// std::map = D-Bus ARRAY of DICT_ENTRY.
template<typename KEY, typename VALUE>
struct DBusSignature<std::map<KEY, VALUE>> {
  // Returns "a{KV}", where "K" and "V" are the signature strings for types
  // KEY/VALUE.
  inline static std::string get() {
    return GetArrayDBusSignature(GetDBusDictEntryType<KEY, VALUE>());
  }
};

// google::protobuf::MessageLite = D-Bus ARRAY of BYTE
template<> struct DBusSignature<google::protobuf::MessageLite> {
  inline static std::string get() {
    return DBusSignature<std::vector<uint8_t>>::get();
  }
};

// The main worker function that returns a signature string for given type T.
// For example, GetDBusSignature<std::map<int, bool>>() would return "a{ib}".
template<typename T>
inline std::string GetDBusSignature() { return DBusSignature<T>::get(); }

//----------------------------------------------------------------------------
// AppendValueToWriter<T>(dbus::MessageWriter* writer, const T& value)
// Write the |value| of type T to D-Bus message.

// Generic template method that fails for all types unless specifically
// specialized for that type.
template<typename T>
inline bool AppendValueToWriter(dbus::MessageWriter* writer, const T& value) {
  LOG(ERROR) << "Serialization of type '"
             << typeid(T).name() << "' not supported";
  return false;
}

// Numerous overloads for various native data types are provided below.

// Basic types.
CHROMEOS_EXPORT bool AppendValueToWriter(dbus::MessageWriter* writer,
                                         bool value);
CHROMEOS_EXPORT bool AppendValueToWriter(dbus::MessageWriter* writer,
                                         uint8_t value);
CHROMEOS_EXPORT bool AppendValueToWriter(dbus::MessageWriter* writer,
                                         int16_t value);
CHROMEOS_EXPORT bool AppendValueToWriter(dbus::MessageWriter* writer,
                                         uint16_t value);
CHROMEOS_EXPORT bool AppendValueToWriter(dbus::MessageWriter* writer,
                                         int32_t value);
CHROMEOS_EXPORT bool AppendValueToWriter(dbus::MessageWriter* writer,
                                         uint32_t value);
CHROMEOS_EXPORT bool AppendValueToWriter(dbus::MessageWriter* writer,
                                         int64_t value);
CHROMEOS_EXPORT bool AppendValueToWriter(dbus::MessageWriter* writer,
                                         uint64_t value);
CHROMEOS_EXPORT bool AppendValueToWriter(dbus::MessageWriter* writer,
                                         double value);
CHROMEOS_EXPORT bool AppendValueToWriter(dbus::MessageWriter* writer,
                                         const std::string& value);
CHROMEOS_EXPORT bool AppendValueToWriter(dbus::MessageWriter* writer,
                                         const char* value);
CHROMEOS_EXPORT bool AppendValueToWriter(dbus::MessageWriter* writer,
                                         const dbus::ObjectPath& value);
CHROMEOS_EXPORT bool AppendValueToWriter(dbus::MessageWriter* writer,
                                         const dbus::FileDescriptor& value);
CHROMEOS_EXPORT bool AppendValueToWriter(dbus::MessageWriter* writer,
                                         const chromeos::Any& value);

// Overloads of AppendValueToWriter for some compound data types such
// as ARRAYs, STRUCTs, DICTs.

// std::vector = D-Bus ARRAY.
template<typename T>
inline bool AppendValueToWriter(dbus::MessageWriter* writer,
                         const std::vector<T>& value) {
  std::string data_type = GetDBusSignature<T>();
  if (data_type.empty())
    return false;
  dbus::MessageWriter array_writer(nullptr);
  writer->OpenArray(data_type, &array_writer);
  bool success = true;
  for (const auto& element : value) {
    if (!AppendValueToWriter(&array_writer, element)) {
      success = false;
      break;
    }
  }
  writer->CloseContainer(&array_writer);
  return success;
}

// std::pair = D-Bus STRUCT with two elements.
template<typename U, typename V>
inline bool AppendValueToWriter(dbus::MessageWriter* writer,
                         const std::pair<U, V>& value) {
  dbus::MessageWriter struct_writer(nullptr);
  writer->OpenStruct(&struct_writer);
  bool success = AppendValueToWriter(&struct_writer, value.first) &&
                 AppendValueToWriter(&struct_writer, value.second);
  writer->CloseContainer(&struct_writer);
  return success;
}

// std::map = D-Bus ARRAY of DICT_ENTRY.
template<typename KEY, typename VALUE>
inline bool AppendValueToWriter(dbus::MessageWriter* writer,
                                const std::map<KEY, VALUE>& value) {
  dbus::MessageWriter dict_writer(nullptr);
  writer->OpenArray(GetDBusDictEntryType<KEY, VALUE>(), &dict_writer);
  bool success = true;
  for (const auto& pair : value) {
    dbus::MessageWriter entry_writer(nullptr);
    dict_writer.OpenDictEntry(&entry_writer);
    success = AppendValueToWriter(&entry_writer, pair.first) &&
              AppendValueToWriter(&entry_writer, pair.second);
    dict_writer.CloseContainer(&entry_writer);
    if (!success)
      break;
  }
  writer->CloseContainer(&dict_writer);
  return success;
}

// google::protobuf::MessageLite = D-Bus ARRAY of BYTE
inline bool AppendValueToWriter(dbus::MessageWriter* writer,
                                const google::protobuf::MessageLite& value) {
  writer->AppendProtoAsArrayOfBytes(value);
  return true;
}

//----------------------------------------------------------------------------
// AppendValueToWriterAsVariant<T>(dbus::MessageWriter* writer, const T& value)
// Write the |value| of type T to D-Bus message as a VARIANT
template<typename T>
inline bool AppendValueToWriterAsVariant(dbus::MessageWriter* writer,
                                         const T& value) {
  std::string data_type = GetDBusSignature<T>();
  if (data_type.empty())
    return false;
  dbus::MessageWriter variant_writer(nullptr);
  writer->OpenVariant(data_type, &variant_writer);
  bool success = AppendValueToWriter(&variant_writer, value);
  writer->CloseContainer(&variant_writer);
  return success;
}

// Special case: do not allow to write a Variant containing a Variant.
// Just redirect to normal AppendValueToWriter().
inline bool AppendValueToWriterAsVariant(dbus::MessageWriter* writer,
                                         const chromeos::Any& value) {
  return AppendValueToWriter(writer, value);
}

//----------------------------------------------------------------------------
// PopValueFromReader<T>(dbus::MessageWriter* writer, T* value)
// Reads the |value| of type T from D-Bus message. These methods can read both
// actual data of type T and data of type T sent over D-Bus as a Variant.
// This allows it, for example, to read a generic dictionary ("a{sv}") as
// a specific map type (e.g. "a{ss}", if a map of string-to-string is expected).

namespace details {
// Helper method used by the many overloads of PopValueFromReader().
// If the current value in the reader is of Variant type, the method descends
// into the Variant and updates the |*reader_ref| with the transient
// |variant_reader| MessageReader instance passed in.
// Returns false if it fails to descend into the Variant.
inline bool DescendIntoVariantIfPresent(
    dbus::MessageReader** reader_ref,
    dbus::MessageReader* variant_reader) {
  if ((*reader_ref)->GetDataType() != dbus::Message::VARIANT)
    return true;
  if (!(*reader_ref)->PopVariant(variant_reader))
    return false;
  *reader_ref = variant_reader;
  return true;
}
}  // namespace details

// Generic template method that fails for all types unless specifically
// specialized for that type.
template<typename T>
inline bool PopValueFromReader(dbus::MessageReader* reader, T* value) {
  LOG(ERROR) << "De-serialization of type '"
             << typeid(T).name() << "' not supported";
  return false;
}

// Numerous overloads for various native data types are provided below.

CHROMEOS_EXPORT bool PopValueFromReader(dbus::MessageReader* reader,
                                        bool* value);
CHROMEOS_EXPORT bool PopValueFromReader(dbus::MessageReader* reader,
                                        uint8_t* value);
CHROMEOS_EXPORT bool PopValueFromReader(dbus::MessageReader* reader,
                                        int16_t* value);
CHROMEOS_EXPORT bool PopValueFromReader(dbus::MessageReader* reader,
                                        uint16_t* value);
CHROMEOS_EXPORT bool PopValueFromReader(dbus::MessageReader* reader,
                                        int32_t* value);
CHROMEOS_EXPORT bool PopValueFromReader(dbus::MessageReader* reader,
                                        uint32_t* value);
CHROMEOS_EXPORT bool PopValueFromReader(dbus::MessageReader* reader,
                                        int64_t* value);
CHROMEOS_EXPORT bool PopValueFromReader(dbus::MessageReader* reader,
                                        uint64_t* value);
CHROMEOS_EXPORT bool PopValueFromReader(dbus::MessageReader* reader,
                                        double* value);
CHROMEOS_EXPORT bool PopValueFromReader(dbus::MessageReader* reader,
                                        std::string* value);
CHROMEOS_EXPORT bool PopValueFromReader(dbus::MessageReader* reader,
                                        dbus::ObjectPath* value);
CHROMEOS_EXPORT bool PopValueFromReader(dbus::MessageReader* reader,
                                        dbus::FileDescriptor* value);
CHROMEOS_EXPORT bool PopValueFromReader(dbus::MessageReader* reader,
                                        chromeos::Any* value);

// Overloads of PopValueFromReader for some compound data types such
// as ARRAYs, STRUCTs, DICTs.

// std::vector = D-Bus ARRAY.
template<typename T>
inline bool PopValueFromReader(dbus::MessageReader* reader,
                               std::vector<T>* value) {
  dbus::MessageReader variant_reader(nullptr);
  dbus::MessageReader array_reader(nullptr);
  if (!details::DescendIntoVariantIfPresent(&reader, &variant_reader) ||
      !reader->PopArray(&array_reader))
    return false;
  value->clear();
  while (array_reader.HasMoreData()) {
    T data;
    if (!PopValueFromReader(&array_reader, &data))
      return false;
    value->push_back(std::move(data));
  }
  return true;
}

// std::pair = D-Bus STRUCT with two elements.
template<typename U, typename V>
inline bool PopValueFromReader(dbus::MessageReader* reader,
                               std::pair<U, V>* value) {
  dbus::MessageReader variant_reader(nullptr);
  dbus::MessageReader struct_reader(nullptr);
  if (!details::DescendIntoVariantIfPresent(&reader, &variant_reader) ||
      !reader->PopStruct(&struct_reader))
    return false;
  return PopValueFromReader(&struct_reader, &value->first) &&
         PopValueFromReader(&struct_reader, &value->second);
}

// std::map = D-Bus ARRAY of DICT_ENTRY.
template<typename KEY, typename VALUE>
inline bool PopValueFromReader(dbus::MessageReader* reader,
                               std::map<KEY, VALUE>* value) {
  dbus::MessageReader variant_reader(nullptr);
  dbus::MessageReader array_reader(nullptr);
  if (!details::DescendIntoVariantIfPresent(&reader, &variant_reader) ||
      !reader->PopArray(&array_reader))
    return false;
  value->clear();
  while (array_reader.HasMoreData()) {
    dbus::MessageReader dict_entry_reader(nullptr);
    if (!array_reader.PopDictEntry(&dict_entry_reader))
      return false;
    KEY key;
    VALUE data;
    if (!PopValueFromReader(&dict_entry_reader, &key) ||
        !PopValueFromReader(&dict_entry_reader, &data))
      return false;
    value->insert(std::make_pair(std::move(key), std::move(data)));
  }
  return true;
}

// google::protobuf::MessageLite = D-Bus ARRAY of BYTE
inline bool PopValueFromReader(dbus::MessageReader* reader,
                               google::protobuf::MessageLite* value) {
  return reader->PopArrayOfBytesAsProto(value);
}

//----------------------------------------------------------------------------
// PopVariantValueFromReader<T>(dbus::MessageWriter* writer, T* value)
// Reads a Variant containing the |value| of type T from D-Bus message.
// Note that the generic PopValueFromReader<T>(...) can do this too.
// This method is provided for two reasons:
//   1. For API symmetry with AppendValueToWriter/AppendValueToWriterAsVariant.
//   2. To be used when it is important to assert that the data was sent
//      specifically as a Variant.
template<typename T>
inline bool PopVariantValueFromReader(dbus::MessageReader* reader, T* value) {
  dbus::MessageReader variant_reader(nullptr);
  if (!reader->PopVariant(&variant_reader))
    return false;
  return PopValueFromReader(&variant_reader, value);
}

// Special handling of request to read a Variant of Variant.
inline bool PopVariantValueFromReader(dbus::MessageReader* reader, Any* value) {
  return PopValueFromReader(reader, value);
}

}  // namespace dbus_utils
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_DBUS_DATA_SERIALIZATION_H_
