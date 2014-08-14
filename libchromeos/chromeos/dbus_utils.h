// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_DBUS_UTILS_H_
#define LIBCHROMEOS_CHROMEOS_DBUS_UTILS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/memory/scoped_ptr.h>
#include <base/values.h>
#include <dbus/message.h>

namespace chromeos {

namespace dbus_utils {

scoped_ptr<dbus::Response> GetBadArgsError(dbus::MethodCall* method_call,
                                           const std::string& message);

using Dictionary = std::map<std::string, std::unique_ptr<base::Value>>;

// Write values to DBus message.
void AppendValueToWriter(dbus::MessageWriter* writer, bool value);
void AppendValueToWriter(dbus::MessageWriter* writer, uint8_t value);
void AppendValueToWriter(dbus::MessageWriter* writer, int16_t value);
void AppendValueToWriter(dbus::MessageWriter* writer, uint16_t value);
void AppendValueToWriter(dbus::MessageWriter* writer, int32_t value);
void AppendValueToWriter(dbus::MessageWriter* writer, uint32_t value);
void AppendValueToWriter(dbus::MessageWriter* writer, int64_t value);
void AppendValueToWriter(dbus::MessageWriter* writer, uint64_t value);
void AppendValueToWriter(dbus::MessageWriter* writer, double value);
void AppendValueToWriter(dbus::MessageWriter* writer, const std::string& value);
void AppendValueToWriter(dbus::MessageWriter* writer,
                         const dbus::ObjectPath& value);
void AppendValueToWriter(dbus::MessageWriter* writer,
                         const std::vector<std::string>& value);
void AppendValueToWriter(dbus::MessageWriter* writer,
                         const std::vector<dbus::ObjectPath>& value);
void AppendValueToWriter(dbus::MessageWriter* writer,
                         const std::vector<uint8_t>& value);
void AppendValueToWriter(dbus::MessageWriter* writer, const Dictionary& value);

// Write values to DBus message as a Variant data type.
void AppendValueToWriterAsVariant(dbus::MessageWriter* writer, bool value);
void AppendValueToWriterAsVariant(dbus::MessageWriter* writer, uint8_t value);
void AppendValueToWriterAsVariant(dbus::MessageWriter* writer, int16_t value);
void AppendValueToWriterAsVariant(dbus::MessageWriter* writer, uint16_t value);
void AppendValueToWriterAsVariant(dbus::MessageWriter* writer, int32_t value);
void AppendValueToWriterAsVariant(dbus::MessageWriter* writer, uint32_t value);
void AppendValueToWriterAsVariant(dbus::MessageWriter* writer, int64_t value);
void AppendValueToWriterAsVariant(dbus::MessageWriter* writer, uint64_t value);
void AppendValueToWriterAsVariant(dbus::MessageWriter* writer, double value);
void AppendValueToWriterAsVariant(dbus::MessageWriter* writer,
                                  const std::string& value);
void AppendValueToWriterAsVariant(dbus::MessageWriter* writer,
                                  const dbus::ObjectPath& value);
void AppendValueToWriterAsVariant(dbus::MessageWriter* writer,
                                  const std::vector<std::string>& value);
void AppendValueToWriterAsVariant(dbus::MessageWriter* writer,
                                  const std::vector<dbus::ObjectPath>& value);
void AppendValueToWriterAsVariant(dbus::MessageWriter* writer,
                                  const std::vector<uint8_t>& value);
void AppendValueToWriterAsVariant(dbus::MessageWriter* writer,
                                  const Dictionary& value);

// Read values from DBus message.
bool PopValueFromReader(dbus::MessageReader* reader, bool* value);
bool PopValueFromReader(dbus::MessageReader* reader, uint8_t* value);
bool PopValueFromReader(dbus::MessageReader* reader, int16_t* value);
bool PopValueFromReader(dbus::MessageReader* reader, uint16_t* value);
bool PopValueFromReader(dbus::MessageReader* reader, int32_t* value);
bool PopValueFromReader(dbus::MessageReader* reader, uint32_t* value);
bool PopValueFromReader(dbus::MessageReader* reader, int64_t* value);
bool PopValueFromReader(dbus::MessageReader* reader, uint64_t* value);
bool PopValueFromReader(dbus::MessageReader* reader, double* value);
bool PopValueFromReader(dbus::MessageReader* reader, std::string* value);
bool PopValueFromReader(dbus::MessageReader* reader, dbus::ObjectPath* value);
bool PopValueFromReader(dbus::MessageReader* reader,
                        std::vector<std::string>* value);
bool PopValueFromReader(dbus::MessageReader* reader,
                        std::vector<dbus::ObjectPath>* value);
bool PopValueFromReader(dbus::MessageReader* reader,
                        std::vector<uint8_t>* value);
bool PopValueFromReader(dbus::MessageReader* reader, Dictionary* value);



}  // namespace dbus_utils

}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_DBUS_UTILS_H_
